[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [string]$ExpectedPublisherSha256 = '',
    [string]$ExpectedVersion = '3.0.0',
    [switch]$KeepInstall,
    [switch]$MachineInstall,
    [switch]$SkipRuntimeSmoke,
    [string]$OutputJson = ''
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom fresh-install QA failed: $Message" }
function CertSha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Quote-Arg([string]$Value) { return '"' + $Value.Replace('"','\"') + '"' }
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
function Test-Version([string]$Actual, [string]$Expected) {
    $pattern = '^' + [regex]::Escape($Expected) + '(?:\.0)?$'
    return ($Actual -match $pattern)
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { Fail 'this QA script must execute on Windows' }
if ($ExpectedVersion -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}(?:-[A-Za-z0-9.-]{1,24})?$') { Fail 'ExpectedVersion must be a semantic version' }
if (-not (Test-Path -LiteralPath $Installer -PathType Leaf)) { Fail 'installer not found' }
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$sig = Get-AuthenticodeSignature -LiteralPath $installerPath
if ($sig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $sig.SignerCertificate) { Fail "installer Authenticode status is $($sig.Status)" }
$publisher = CertSha256 $sig.SignerCertificate
if ($ExpectedPublisherSha256) {
    if ($ExpectedPublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'expected publisher SHA-256 must be 64 hex characters' }
    if ($publisher -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'installer publisher fingerprint mismatch' }
}

$qaRoot = Join-Path $env:TEMP ('WebCom-FreshInstall-QA-' + [Guid]::NewGuid().ToString('N'))
$installDir = Join-Path $qaRoot 'WebCom'
$acceptanceRoot = $null
$browserProcess = $null
$previousQaMode = $env:WEBCOM_QA_MODE
$runtime = [ordered]@{ skipped = [bool]$SkipRuntimeSmoke; windowReady = $false; webViewProcessCount = 0; exitCode = $null }
$uninstalled = $false
$registryClean = $false
$installedExecutableHash = ''
New-Item -ItemType Directory -Force -Path $qaRoot | Out-Null
try {
    $mode = if ($MachineInstall) { '/ALLUSERS' } else { '/CURRENTUSER' }
    if ($MachineInstall) {
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = New-Object Security.Principal.WindowsPrincipal($identity)
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            Fail 'machine-install QA must run from an elevated PowerShell session'
        }
    }

    $process = Start-Process -FilePath $installerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',$mode,"/DIR=$installDir") -Wait -PassThru
    if ($process.ExitCode -ne 0) { Fail "installer exited with code $($process.ExitCode)" }
    foreach ($name in 'WebCom.exe','WebView2Loader.dll','unins000.exe') {
        if (-not (Test-Path -LiteralPath (Join-Path $installDir $name) -PathType Leaf)) { Fail "installed file missing: $name" }
    }

    $installedExe = Join-Path $installDir 'WebCom.exe'
    $exeSig = Get-AuthenticodeSignature -LiteralPath $installedExe
    if ($exeSig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $exeSig.SignerCertificate) { Fail "installed WebCom.exe signature status is $($exeSig.Status)" }
    $exePublisher = CertSha256 $exeSig.SignerCertificate
    if ($ExpectedPublisherSha256 -and $exePublisher -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'installed WebCom.exe publisher fingerprint mismatch' }
    $versionInfo = (Get-Item -LiteralPath $installedExe).VersionInfo
    if (-not (Test-Version $versionInfo.ProductVersion $ExpectedVersion)) { Fail "installed ProductVersion is '$($versionInfo.ProductVersion)', expected $ExpectedVersion" }
    if (-not (Test-Version $versionInfo.FileVersion $ExpectedVersion)) { Fail "installed FileVersion is '$($versionInfo.FileVersion)', expected $ExpectedVersion" }
    $installedExecutableHash = (Get-FileHash -LiteralPath $installedExe -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "Installed executable signature/version verified: $ExpectedVersion" -ForegroundColor Green

    $registryRoot = if ($MachineInstall) { 'Registry::HKEY_LOCAL_MACHINE' } else { 'Registry::HKEY_CURRENT_USER' }
    $registeredApps = Join-Path $registryRoot 'Software\RegisteredApplications'
    $capabilities = Join-Path $registryRoot 'Software\WebCom\Capabilities'
    $urlAssociations = Join-Path $capabilities 'UrlAssociations'
    $fileAssociations = Join-Path $capabilities 'FileAssociations'
    $progId = Join-Path $registryRoot 'Software\Classes\WebCom.Url'
    $htmlProgId = Join-Path $registryRoot 'Software\Classes\WebCom.HTML'
    $pdfProgId = Join-Path $registryRoot 'Software\Classes\WebCom.PDF'
    $openCommand = Join-Path $progId 'shell\open\command'
    $htmlOpenCommand = Join-Path $htmlProgId 'shell\open\command'
    $pdfOpenCommand = Join-Path $pdfProgId 'shell\open\command'
    $applicationKey = Join-Path $registryRoot 'Software\Classes\Applications\WebCom.exe'
    $registered = (Get-ItemProperty -LiteralPath $registeredApps -Name 'WebCom' -ErrorAction Stop).WebCom
    if ($registered -ne 'Software\WebCom\Capabilities') { Fail 'RegisteredApplications WebCom capability path is incorrect' }
    $associations = Get-ItemProperty -LiteralPath $urlAssociations -ErrorAction Stop
    if ($associations.http -ne 'WebCom.Url' -or $associations.https -ne 'WebCom.Url') { Fail 'HTTP/HTTPS capability registration is incomplete' }
    $files = Get-ItemProperty -LiteralPath $fileAssociations -ErrorAction Stop
    if ($files.'.html' -ne 'WebCom.HTML' -or $files.'.htm' -ne 'WebCom.HTML' -or $files.'.pdf' -ne 'WebCom.PDF') { Fail 'HTML/PDF capability registration is incomplete' }
    $command = (Get-Item -LiteralPath $openCommand -ErrorAction Stop).GetValue('')
    $htmlCommand = (Get-Item -LiteralPath $htmlOpenCommand -ErrorAction Stop).GetValue('')
    $pdfCommand = (Get-Item -LiteralPath $pdfOpenCommand -ErrorAction Stop).GetValue('')
    if (-not $command -or $command -notlike ('*' + $installedExe + '*')) { Fail 'WebCom.Url open command does not target the installed executable' }
    if (-not $htmlCommand -or $htmlCommand -notlike ('*' + $installedExe + '*')) { Fail 'WebCom.HTML open command does not target the installed executable' }
    if (-not $pdfCommand -or $pdfCommand -notlike ('*' + $installedExe + '*')) { Fail 'WebCom.PDF open command does not target the installed executable' }
    Write-Host "Default Apps registration verified in $registryRoot." -ForegroundColor Green

    if (-not $SkipRuntimeSmoke) {
        $acceptanceRoot = Join-Path $env:TEMP ('WebCom-Acceptance-FreshInstall-' + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $acceptanceRoot -Force | Out-Null
        $smokePage = Join-Path $acceptanceRoot 'fresh-install.html'
        '<!doctype html><meta charset="utf-8"><title>WebCom fresh-install acceptance</title><h1>WebCom fresh-install acceptance</h1>' | Set-Content -LiteralPath $smokePage -Encoding UTF8
        $env:WEBCOM_QA_MODE = '1'
        $arguments = @('--qa-data-root',(Quote-Arg $acceptanceRoot),'--profile','Acceptance','--url',(Quote-Arg $smokePage))
        $browserProcess = Start-Process -FilePath $installedExe -ArgumentList $arguments -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            Start-Sleep -Milliseconds 250
            $browserProcess.Refresh()
            if ($browserProcess.HasExited) { Fail "installed browser exited during runtime smoke with $($browserProcess.ExitCode)" }
            if ($browserProcess.MainWindowHandle -ne 0) { $runtime.windowReady = $true }
            $runtime.webViewProcessCount = Get-DescendantWebViewCount $browserProcess.Id
            if ($runtime.windowReady -and $runtime.webViewProcessCount -gt 0) { break }
        } while ([DateTime]::UtcNow -lt $deadline)
        if (-not $runtime.windowReady) { Fail 'installed browser did not create a top-level window within 30 seconds' }
        if ($runtime.webViewProcessCount -le 0) { Fail 'installed browser opened but no descendant WebView2 runtime process was observed' }
        Stop-QAProcess $browserProcess
        $browserProcess.Refresh()
        if ($browserProcess.HasExited) { $runtime.exitCode = $browserProcess.ExitCode }
        $browserProcess = $null
        Write-Host "Installed runtime smoke passed; WebView2 processes observed: $($runtime.webViewProcessCount)" -ForegroundColor Green
    }

    if (-not $KeepInstall) {
        $uninstall = Join-Path $installDir 'unins000.exe'
        $un = Start-Process -FilePath $uninstall -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait -PassThru
        if ($un.ExitCode -ne 0) { Fail "uninstaller exited with code $($un.ExitCode)" }
        $uninstalled = $true
        if ((Get-ItemProperty -LiteralPath $registeredApps -Name 'WebCom' -ErrorAction SilentlyContinue).WebCom) { Fail 'RegisteredApplications WebCom value remained after uninstall' }
        if (Test-Path -LiteralPath $progId) { Fail 'WebCom.Url ProgID remained after uninstall' }
        if (Test-Path -LiteralPath $htmlProgId) { Fail 'WebCom.HTML ProgID remained after uninstall' }
        if (Test-Path -LiteralPath $pdfProgId) { Fail 'WebCom.PDF ProgID remained after uninstall' }
        if (Test-Path -LiteralPath $applicationKey) { Fail 'Applications\WebCom.exe registration remained after uninstall' }
        $registryClean = $true
        Write-Host 'Silent uninstall and Default Apps registration cleanup completed.' -ForegroundColor Green
    }

    if ($OutputJson) {
        $resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputJson)
        $outputParent = Split-Path -Parent $resolvedOutput
        if ($outputParent) { New-Item -ItemType Directory -Path $outputParent -Force | Out-Null }
        $summary = [ordered]@{
            schema = 'webcom-fresh-install-v2'
            generatedAt = (Get-Date).ToString('o')
            installer = $installerPath
            installerSha256 = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLowerInvariant()
            expectedVersion = $ExpectedVersion
            installedProductVersion = $versionInfo.ProductVersion
            installedFileVersion = $versionInfo.FileVersion
            installedExecutableSha256 = $installedExecutableHash
            publisherSha256 = $exePublisher
            machineInstall = [bool]$MachineInstall
            runtimeSmoke = $runtime
            uninstalled = $uninstalled
            registryCleanupVerified = $registryClean
        }
        $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedOutput -Encoding UTF8
        Write-Host "Fresh-install report: $resolvedOutput"
    }
}
finally {
    if ($previousQaMode -eq $null) { Remove-Item Env:WEBCOM_QA_MODE -ErrorAction SilentlyContinue }
    else { $env:WEBCOM_QA_MODE = $previousQaMode }
    if ($browserProcess) { Stop-QAProcess $browserProcess }
    if ($acceptanceRoot) { Remove-Item -LiteralPath $acceptanceRoot -Recurse -Force -ErrorAction SilentlyContinue }
    if (-not $KeepInstall) { Remove-Item -LiteralPath $qaRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
