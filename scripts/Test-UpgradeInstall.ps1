[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PreviousInstaller,
    [Parameter(Mandatory=$true)][string]$CurrentInstaller,
    [Parameter(Mandatory=$true)][string]$ExpectedPublisherSha256,
    [string]$ExpectedVersion = '3.0.0',
    [switch]$KeepInstall,
    [string]$OutputJson = ''
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom upgrade-install QA failed: $Message" }
function CertSha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Quote-Arg([string]$Value) { return '"' + $Value.Replace('"','\"') + '"' }
function Verify-SignedInstaller([string]$Path, [string]$PublisherPin) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "installer not found: $Path" }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $sig = Get-AuthenticodeSignature -LiteralPath $resolved
    if ($sig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $sig.SignerCertificate) { Fail "installer Authenticode status is $($sig.Status): $resolved" }
    $publisher = CertSha256 $sig.SignerCertificate
    if ($publisher -ne $PublisherPin.ToLowerInvariant()) { Fail "installer publisher fingerprint mismatch: $resolved" }
    return $resolved
}
function Get-DescendantWebViewCount([int]$RootPid) {
    $snapshot = @(Get-CimInstance Win32_Process -ErrorAction Stop | Select-Object ProcessId,ParentProcessId,Name,CreationDate)
    $index = @{}
    foreach ($entry in $snapshot) { $index[[int]$entry.ProcessId] = $entry }
    $count = 0
    foreach ($candidate in $snapshot) {
        if ($candidate.Name -ine 'msedgewebview2.exe') { continue }
        $current = $candidate
        $visited = [Collections.Generic.HashSet[int]]::new()
        while ($current -and $visited.Add([int]$current.ProcessId)) {
            if ([int]$current.ProcessId -eq $RootPid) { ++$count; break }
            if (-not $index.ContainsKey([int]$current.ParentProcessId)) { break }
            $parent = $index[[int]$current.ParentProcessId]
            if ($parent.CreationDate -and $current.CreationDate -and $parent.CreationDate -gt $current.CreationDate) { break }
            $current = $parent
        }
    }
    return $count
}
function Stop-QAProcess($Process) {
    if (-not $Process) { return }
    try { $Process.Refresh() } catch { return }
    if ($Process.HasExited) { return }
    try { [void]$Process.CloseMainWindow() } catch {}
    if (-not $Process.WaitForExit(5000)) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        [void]$Process.WaitForExit(5000)
    }
}
function Invoke-RuntimeSmoke([string]$Exe, [string]$DataRoot, [string]$Page, [string]$Label) {
    $arguments = @('--qa-data-root',(Quote-Arg $DataRoot),'--profile','Acceptance','--url',(Quote-Arg $Page))
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru
    $windowReady = $false
    $webViewCount = 0
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            Start-Sleep -Milliseconds 250
            $process.Refresh()
            if ($process.HasExited) { Fail "$Label browser exited during runtime smoke with $($process.ExitCode)" }
            if ($process.MainWindowHandle -ne 0) { $windowReady = $true }
            $webViewCount = Get-DescendantWebViewCount $process.Id
            if ($windowReady -and $webViewCount -gt 0) { break }
        } while ([DateTime]::UtcNow -lt $deadline)
        if (-not $windowReady) { Fail "$Label browser did not create a top-level window within 30 seconds" }
        if ($webViewCount -le 0) { Fail "$Label browser opened but no descendant WebView2 runtime process was observed" }
        return [ordered]@{ windowReady = $windowReady; webViewProcessCount = $webViewCount; processId = $process.Id }
    }
    finally { Stop-QAProcess $process }
}
function Test-Version([string]$Actual, [string]$Expected) {
    $pattern = '^' + [regex]::Escape($Expected) + '(?:\.0)?$'
    return ($Actual -match $pattern)
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { Fail 'this QA script must execute on Windows' }
if ($ExpectedPublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'ExpectedPublisherSha256 must be 64 hex characters' }
if ($ExpectedVersion -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}(?:-[A-Za-z0-9.-]{1,24})?$') { Fail 'ExpectedVersion must be a semantic version' }
$previousPath = Verify-SignedInstaller $PreviousInstaller $ExpectedPublisherSha256
$currentPath = Verify-SignedInstaller $CurrentInstaller $ExpectedPublisherSha256
$previousHash = (Get-FileHash -LiteralPath $previousPath -Algorithm SHA256).Hash.ToLowerInvariant()
$currentHash = (Get-FileHash -LiteralPath $currentPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($previousHash -eq $currentHash) { Fail 'PreviousInstaller and CurrentInstaller are byte-identical; upgrade QA requires two distinct packages' }

$installRoot = Join-Path $env:TEMP ('WebCom-UpgradeInstall-QA-' + [Guid]::NewGuid().ToString('N'))
$installDir = Join-Path $installRoot 'WebCom'
$acceptanceRoot = Join-Path $env:TEMP ('WebCom-Acceptance-Upgrade-' + [Guid]::NewGuid().ToString('N'))
$smokePage = Join-Path $acceptanceRoot 'upgrade.html'
$previousQaMode = $env:WEBCOM_QA_MODE
$previousRuntime = $null
$currentRuntime = $null
$previousVersion = ''
$currentVersion = ''
$stateHashBeforeUpgrade = ''
$stateHashAfterInstaller = ''
$registryClean = $false
New-Item -ItemType Directory -Path $installRoot -Force | Out-Null
New-Item -ItemType Directory -Path $acceptanceRoot -Force | Out-Null
'<!doctype html><meta charset="utf-8"><title>WebCom upgrade acceptance</title><h1>WebCom upgrade acceptance</h1>' | Set-Content -LiteralPath $smokePage -Encoding UTF8
try {
    $env:WEBCOM_QA_MODE = '1'
    Write-Host '=== Install previous package ===' -ForegroundColor Cyan
    $previousInstall = Start-Process -FilePath $previousPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CURRENTUSER',"/DIR=$installDir") -Wait -PassThru
    if ($previousInstall.ExitCode -ne 0) { Fail "previous installer exited with code $($previousInstall.ExitCode)" }
    $installedExe = Join-Path $installDir 'WebCom.exe'
    if (-not (Test-Path -LiteralPath $installedExe -PathType Leaf)) { Fail 'previous installation did not create WebCom.exe' }
    $previousInfo = (Get-Item -LiteralPath $installedExe).VersionInfo
    $previousVersion = $previousInfo.ProductVersion
    $previousRuntime = Invoke-RuntimeSmoke $installedExe $acceptanceRoot $smokePage 'previous'

    $statePath = Join-Path $acceptanceRoot 'state.v1'
    if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) { Fail 'previous WebCom runtime did not persist state.v1 in the isolated QA profile' }
    if ((Get-Item -LiteralPath $statePath).Length -le 0) { Fail 'previous WebCom runtime created an empty state.v1' }
    $stateHashBeforeUpgrade = (Get-FileHash -LiteralPath $statePath -Algorithm SHA256).Hash.ToLowerInvariant()

    Write-Host '=== Upgrade in place to current package ===' -ForegroundColor Cyan
    $currentInstall = Start-Process -FilePath $currentPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CURRENTUSER',"/DIR=$installDir") -Wait -PassThru
    if ($currentInstall.ExitCode -ne 0) { Fail "current installer exited with code $($currentInstall.ExitCode)" }
    $currentInfo = (Get-Item -LiteralPath $installedExe).VersionInfo
    $currentVersion = $currentInfo.ProductVersion
    if (-not (Test-Version $currentInfo.ProductVersion $ExpectedVersion)) { Fail "upgraded ProductVersion is '$($currentInfo.ProductVersion)', expected $ExpectedVersion" }
    if (-not (Test-Version $currentInfo.FileVersion $ExpectedVersion)) { Fail "upgraded FileVersion is '$($currentInfo.FileVersion)', expected $ExpectedVersion" }
    $exeSig = Get-AuthenticodeSignature -LiteralPath $installedExe
    if ($exeSig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $exeSig.SignerCertificate) { Fail "upgraded WebCom.exe signature status is $($exeSig.Status)" }
    if ((CertSha256 $exeSig.SignerCertificate) -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'upgraded WebCom.exe publisher fingerprint mismatch' }

    if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) { Fail 'current installer removed the isolated previous-version state.v1' }
    $stateHashAfterInstaller = (Get-FileHash -LiteralPath $statePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($stateHashAfterInstaller -ne $stateHashBeforeUpgrade) { Fail 'current installer modified browser profile state before the upgraded browser was launched' }

    $currentRuntime = Invoke-RuntimeSmoke $installedExe $acceptanceRoot $smokePage 'current'
    if (-not (Test-Path -LiteralPath $statePath -PathType Leaf) -or (Get-Item -LiteralPath $statePath).Length -le 0) { Fail 'upgraded browser did not preserve a readable isolated state.v1' }
    Write-Host "Upgrade runtime passed: $previousVersion -> $currentVersion" -ForegroundColor Green

    if (-not $KeepInstall) {
        $uninstall = Join-Path $installDir 'unins000.exe'
        if (-not (Test-Path -LiteralPath $uninstall -PathType Leaf)) { Fail 'upgraded installation is missing unins000.exe' }
        $un = Start-Process -FilePath $uninstall -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait -PassThru
        if ($un.ExitCode -ne 0) { Fail "uninstaller exited with code $($un.ExitCode)" }
        $registeredApps = 'Registry::HKEY_CURRENT_USER\Software\RegisteredApplications'
        $progId = 'Registry::HKEY_CURRENT_USER\Software\Classes\WebCom.Url'
        $htmlProgId = 'Registry::HKEY_CURRENT_USER\Software\Classes\WebCom.HTML'
        $pdfProgId = 'Registry::HKEY_CURRENT_USER\Software\Classes\WebCom.PDF'
        $applicationKey = 'Registry::HKEY_CURRENT_USER\Software\Classes\Applications\WebCom.exe'
        if ((Get-ItemProperty -LiteralPath $registeredApps -Name 'WebCom' -ErrorAction SilentlyContinue).WebCom) { Fail 'RegisteredApplications WebCom value remained after upgrade uninstall' }
        if (Test-Path -LiteralPath $progId) { Fail 'WebCom.Url ProgID remained after upgrade uninstall' }
        if (Test-Path -LiteralPath $htmlProgId) { Fail 'WebCom.HTML ProgID remained after upgrade uninstall' }
        if (Test-Path -LiteralPath $pdfProgId) { Fail 'WebCom.PDF ProgID remained after upgrade uninstall' }
        if (Test-Path -LiteralPath $applicationKey) { Fail 'Applications\WebCom.exe registration remained after upgrade uninstall' }
        $registryClean = $true
    }

    if ($OutputJson) {
        $resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputJson)
        $outputParent = Split-Path -Parent $resolvedOutput
        if ($outputParent) { New-Item -ItemType Directory -Path $outputParent -Force | Out-Null }
        $summary = [ordered]@{
            schema = 'webcom-upgrade-install-v1'
            generatedAt = (Get-Date).ToString('o')
            previousInstaller = $previousPath
            currentInstaller = $currentPath
            previousInstallerSha256 = $previousHash
            currentInstallerSha256 = $currentHash
            previousProductVersion = $previousVersion
            currentProductVersion = $currentVersion
            expectedCurrentVersion = $ExpectedVersion
            profileStatePreservedByInstaller = ($stateHashBeforeUpgrade -eq $stateHashAfterInstaller)
            previousRuntimeSmoke = $previousRuntime
            currentRuntimeSmoke = $currentRuntime
            registryCleanupVerified = $registryClean
        }
        $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedOutput -Encoding UTF8
        Write-Host "Upgrade-install report: $resolvedOutput"
    }
}
finally {
    if ($previousQaMode -eq $null) { Remove-Item Env:WEBCOM_QA_MODE -ErrorAction SilentlyContinue }
    else { $env:WEBCOM_QA_MODE = $previousQaMode }
    if (-not $KeepInstall) { Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $acceptanceRoot -Recurse -Force -ErrorAction SilentlyContinue
}
