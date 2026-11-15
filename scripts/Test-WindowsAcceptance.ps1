[CmdletBinding()]
param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64','ARM64')]
    [string]$Architecture = 'x64',
    [switch]$SkipBuild,
    [switch]$SkipUiReplay,
    [switch]$SkipRuntimeSmoke,
    [switch]$AllowCompilerWarnings,
    [switch]$MeasureMemory,
    [ValidateRange(1,100)]
    [int]$MemorySamples = 5,
    [ValidateRange(1,3600)]
    [int]$MemoryWarmupSeconds = 3,
    [ValidateRange(0,1048576)]
    [double]$WorkingSetBudgetMiB = 0,
    [ValidateRange(0,1048576)]
    [double]$PrivateCommitBudgetMiB = 0,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$expectedVersion = '3.0.0'

function Fail([string]$Message) { throw "WebCom Windows acceptance failed: $Message" }
function Ensure-File([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "$Label missing: $Path" }
}
function Quote-Arg([string]$Value) {
    return '"' + $Value.Replace('"','\"') + '"'
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
function Stop-AcceptanceProcess($Process) {
    if (-not $Process) { return }
    try { $Process.Refresh() } catch { return }
    if ($Process.HasExited) { return }
    try { [void]$Process.CloseMainWindow() } catch {}
    if (-not $Process.WaitForExit(5000)) {
        try { Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue } catch {}
        [void]$Process.WaitForExit(5000)
    }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    Fail 'this acceptance runner must execute on Windows'
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot "acceptance\windows-$Architecture-$timestamp"
}
$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$buildLog = Join-Path $repositoryRoot 'build-last.log'
$buildDirectory = Join-Path $repositoryRoot "build\windows-$Architecture-$Configuration"
$packageDirectory = Join-Path $repositoryRoot $(if ($Architecture -eq 'x64') { 'dist\WebCom' } else { 'dist\WebCom-arm64' })

if (-not $SkipBuild) {
    Write-Host '=== Clean Windows build ===' -ForegroundColor Cyan
    Remove-Item -LiteralPath $buildDirectory -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $packageDirectory -Recurse -Force -ErrorAction SilentlyContinue
    Push-Location $repositoryRoot
    try {
        $buildArgs = @('-Configuration', $Configuration, '-Architecture', $Architecture)
        if ($AllowCompilerWarnings) { $buildArgs += '-AllowCompilerWarnings' }
        & (Join-Path $repositoryRoot 'build.cmd') @buildArgs
        if ($LASTEXITCODE -ne 0) { Fail "build.cmd exited with $LASTEXITCODE. See $buildLog" }
    }
    finally { Pop-Location }
}

Ensure-File $buildLog 'build log'
$buildText = Get-Content -LiteralPath $buildLog -Raw -Encoding UTF8
if ($buildText -notmatch 'MSVC source-preflight checks, 0 failures') {
    Fail 'build log does not contain a successful MSVC source-preflight result'
}
$repositoryPattern = [regex]::Escape($repositoryRoot)
$compilerWarnings = @([regex]::Matches($buildText, '(?im)^.*\.(?:cpp|cxx|cc|h|hpp|inl)\(\d+\): warning C\d+.*$') |
    ForEach-Object { $_.Value.Trim() } | Where-Object { $_ -match $repositoryPattern })
if ($compilerWarnings.Count -gt 0 -and -not $AllowCompilerWarnings) {
    $warningPath = Join-Path $OutputDirectory 'compiler-warnings.txt'
    $compilerWarnings | Set-Content -LiteralPath $warningPath -Encoding UTF8
    Fail "$($compilerWarnings.Count) MSVC project warning(s) found. See $warningPath"
}

$webComExe = Join-Path $packageDirectory 'WebCom.exe'
$loaderDll = Join-Path $packageDirectory 'WebView2Loader.dll'
$uiTests = Join-Path $packageDirectory 'WebCom-ui-tests.exe'
Ensure-File $webComExe 'packaged browser'
Ensure-File $loaderDll 'WebView2 loader'
$versionInfo = (Get-Item -LiteralPath $webComExe).VersionInfo
if ($versionInfo.ProductVersion -notmatch '^3\.0\.0(?:\.0)?$') {
    Fail "packaged ProductVersion is '$($versionInfo.ProductVersion)', expected $expectedVersion"
}
if ($versionInfo.FileVersion -notmatch '^3\.0\.0(?:\.0)?$') {
    Fail "packaged FileVersion is '$($versionInfo.FileVersion)', expected $expectedVersion"
}

$extensionRoot = Join-Path $packageDirectory 'extensions\webcom-ad-blocker'
foreach ($name in 'manifest.json','network-rules.json','site-config.js','web-ads.css','youtube.css','youtube-player.js') {
    Ensure-File (Join-Path $extensionRoot $name) "bundled blocker asset $name"
}
$extensionManifest = Get-Content -LiteralPath (Join-Path $extensionRoot 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ([int]$extensionManifest.manifest_version -ne 3) { Fail 'bundled ad blocker is not packaged as Manifest V3' }

$uiResult = [ordered]@{ skipped = [bool]$SkipUiReplay; exitCode = $null; stdout = ''; stderr = '' }
if (-not $SkipUiReplay) {
    Write-Host '=== Packaged native UI replay ===' -ForegroundColor Cyan
    Ensure-File $uiTests 'packaged native UI harness'
    $uiStdout = Join-Path $OutputDirectory 'ui-tests.stdout.txt'
    $uiStderr = Join-Path $OutputDirectory 'ui-tests.stderr.txt'
    $uiProcess = Start-Process -FilePath $uiTests -PassThru -RedirectStandardOutput $uiStdout -RedirectStandardError $uiStderr
    if (-not $uiProcess.WaitForExit(120000)) {
        Stop-Process -Id $uiProcess.Id -Force -ErrorAction SilentlyContinue
        Fail 'packaged native UI harness exceeded the 120-second timeout'
    }
    $uiResult.exitCode = $uiProcess.ExitCode
    $uiResult.stdout = $uiStdout
    $uiResult.stderr = $uiStderr
    if ($uiProcess.ExitCode -ne 0) { Fail "packaged native UI harness exited with $($uiProcess.ExitCode)" }
}

$hostArchitecture = if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE }
$crossTarget = ($Architecture -eq 'ARM64' -and $hostArchitecture -notmatch '^ARM64$')
$smokeResult = [ordered]@{ skipped = [bool]($SkipRuntimeSmoke -or $crossTarget); windowReady = $false; webViewProcessCount = 0; exitCode = $null; qaRoot = '' }
$qaRoot = $null
$browserProcess = $null
$previousQaMode = $env:WEBCOM_QA_MODE
try {
    if (-not $SkipRuntimeSmoke -and -not $crossTarget) {
        Write-Host '=== Isolated packaged WebView2 smoke test ===' -ForegroundColor Cyan
        $qaRoot = Join-Path $env:TEMP ('WebCom-Acceptance-' + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $qaRoot -Force | Out-Null
        $smokeResult.qaRoot = $qaRoot
        $smokePage = Join-Path $qaRoot 'smoke.html'
        '<!doctype html><meta charset="utf-8"><title>WebCom acceptance</title><h1>WebCom acceptance</h1><p>Local WebView2 smoke page.</p>' | Set-Content -LiteralPath $smokePage -Encoding UTF8
        $env:WEBCOM_QA_MODE = '1'
        $arguments = @(
            '--qa-data-root', (Quote-Arg $qaRoot),
            '--profile', 'Acceptance',
            '--url', (Quote-Arg $smokePage)
        )
        $browserProcess = Start-Process -FilePath $webComExe -ArgumentList $arguments -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            Start-Sleep -Milliseconds 250
            $browserProcess.Refresh()
            if ($browserProcess.HasExited) { Fail "packaged browser exited during smoke startup with $($browserProcess.ExitCode)" }
            if ($browserProcess.MainWindowHandle -ne 0) { $smokeResult.windowReady = $true }
            $smokeResult.webViewProcessCount = Get-DescendantWebViewCount $browserProcess.Id
            if ($smokeResult.windowReady -and $smokeResult.webViewProcessCount -gt 0) { break }
        } while ([DateTime]::UtcNow -lt $deadline)
        if (-not $smokeResult.windowReady) { Fail 'packaged browser did not create a top-level window within 30 seconds' }
        if ($smokeResult.webViewProcessCount -le 0) { Fail 'packaged browser window opened but no descendant WebView2 runtime process was observed' }

        if ($MeasureMemory) {
            $memoryCsv = Join-Path $OutputDirectory 'memory-one-tab.csv'
            $memoryJson = Join-Path $OutputDirectory 'memory-one-tab.json'
            $measure = Join-Path $PSScriptRoot 'Measure-WebComMemory.ps1'
            $memoryStdout = Join-Path $OutputDirectory 'memory.stdout.txt'
            $memoryStderr = Join-Path $OutputDirectory 'memory.stderr.txt'
            $powerShellExe = Join-Path $PSHOME 'powershell.exe'
            $memoryArguments = @(
                '-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',(Quote-Arg $measure),
                '-RootProcessId',[string]$browserProcess.Id,
                '-Samples',[string]$MemorySamples,
                '-IntervalSeconds','2',
                '-WarmupSeconds',[string]$MemoryWarmupSeconds,
                '-Scenario','one-tab',
                '-WorkingSetBudgetMiB',[string]$WorkingSetBudgetMiB,
                '-PrivateCommitBudgetMiB',[string]$PrivateCommitBudgetMiB,
                '-CsvPath',(Quote-Arg $memoryCsv),
                '-JsonPath',(Quote-Arg $memoryJson)
            )
            if ($WorkingSetBudgetMiB -gt 0 -or $PrivateCommitBudgetMiB -gt 0) { $memoryArguments += '-FailOnBudgetExceeded' }
            $memoryProcess = Start-Process -FilePath $powerShellExe -ArgumentList $memoryArguments -PassThru `
                -RedirectStandardOutput $memoryStdout -RedirectStandardError $memoryStderr
            if (-not $memoryProcess.WaitForExit(180000)) {
                Stop-Process -Id $memoryProcess.Id -Force -ErrorAction SilentlyContinue
                Fail 'memory acceptance exceeded the 180-second timeout'
            }
            if ($memoryProcess.ExitCode -ne 0) { Fail "memory acceptance returned exit code $($memoryProcess.ExitCode); see $memoryStderr" }
        }
    }
}
finally {
    if ($previousQaMode -eq $null) { Remove-Item Env:WEBCOM_QA_MODE -ErrorAction SilentlyContinue }
    else { $env:WEBCOM_QA_MODE = $previousQaMode }
    if ($browserProcess) {
        Stop-AcceptanceProcess $browserProcess
        try { $browserProcess.Refresh(); if ($browserProcess.HasExited) { $smokeResult.exitCode = $browserProcess.ExitCode } } catch {}
    }
    if ($qaRoot) { Remove-Item -LiteralPath $qaRoot -Recurse -Force -ErrorAction SilentlyContinue }
}

$summary = [ordered]@{
    schema = 'webcom-windows-acceptance-v1'
    generatedAt = (Get-Date).ToString('o')
    version = $expectedVersion
    architecture = $Architecture
    configuration = $Configuration
    cleanBuild = (-not $SkipBuild)
    buildLog = $buildLog
    compilerWarningCount = $compilerWarnings.Count
    executable = [ordered]@{
        path = $webComExe
        productVersion = $versionInfo.ProductVersion
        fileVersion = $versionInfo.FileVersion
        sha256 = (Get-FileHash -LiteralPath $webComExe -Algorithm SHA256).Hash.ToLowerInvariant()
        bytes = (Get-Item -LiteralPath $webComExe).Length
    }
    blocker = [ordered]@{ manifestVersion = [int]$extensionManifest.manifest_version; packaged = $true }
    nativeUiReplay = $uiResult
    runtimeSmoke = $smokeResult
    memoryReport = $(if ($MeasureMemory) { Join-Path $OutputDirectory 'memory-one-tab.json' } else { '' })
}
$summaryPath = Join-Path $OutputDirectory 'windows-acceptance.json'
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Copy-Item -LiteralPath $buildLog -Destination (Join-Path $OutputDirectory 'build-last.log') -Force

Write-Host ''
Write-Host 'Windows acceptance passed.' -ForegroundColor Green
Write-Host "Version: $($versionInfo.ProductVersion)"
Write-Host "MSVC project warnings: $($compilerWarnings.Count)"
if (-not $smokeResult.skipped) { Write-Host "Runtime smoke: window ready; WebView2 processes observed: $($smokeResult.webViewProcessCount)" }
Write-Host "Acceptance report: $summaryPath"
