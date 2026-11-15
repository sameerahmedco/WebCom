[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$CurrentInstaller,
    [Parameter(Mandatory=$true)][string]$StableManifest,
    [Parameter(Mandatory=$true)][string]$ExpectedPublisherSha256,
    [string]$ExpectedVersion = '3.0.0',
    [string]$PreviousInstaller = '',
    [switch]$NoPreviousSignedBuild,
    [string]$BetaManifest = '',
    [string]$BetaPackage = '',
    [switch]$SkipMemoryGate,
    [ValidateRange(1,100)][int]$MemorySamples = 10,
    [ValidateRange(1,3600)][int]$MemoryWarmupSeconds = 5,
    [ValidateRange(1,1048576)][double]$WorkingSetBudgetMiB = 300,
    [ValidateRange(0,1048576)][double]$PrivateCommitBudgetMiB = 0,
    [switch]$RunMachineInstall,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom Release Candidate gate failed: $Message" }
function Resolve-File([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "$Label not found: $Path" }
    return (Resolve-Path -LiteralPath $Path).Path
}
function Report-Ref([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "expected report missing: $Path" }
    return [ordered]@{
        path = $Path
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
        bytes = (Get-Item -LiteralPath $Path).Length
    }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { Fail 'this Release Candidate gate must execute on Windows' }
if ($ExpectedPublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'ExpectedPublisherSha256 must be 64 hex characters' }
if ($ExpectedVersion -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}(?:-[A-Za-z0-9.-]{1,24})?$') { Fail 'ExpectedVersion must be a semantic version' }
if ([bool]$BetaManifest -xor [bool]$BetaPackage) { Fail 'BetaManifest and BetaPackage must be supplied together' }
if ($PreviousInstaller -and $NoPreviousSignedBuild) { Fail 'use either PreviousInstaller or NoPreviousSignedBuild, not both' }
if (-not $PreviousInstaller -and -not $NoPreviousSignedBuild) { Fail 'supply PreviousInstaller for upgrade QA, or explicitly pass -NoPreviousSignedBuild when no prior signed WebCom release exists' }

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$currentInstallerPath = Resolve-File $CurrentInstaller 'current installer'
$signedDistReportPath = Resolve-File (Join-Path (Split-Path -Parent $currentInstallerPath) 'signed-dist-verification.json') 'signed dist verification report'
$stableManifestPath = Resolve-File $StableManifest 'stable manifest'
$previousInstallerPath = ''
if ($PreviousInstaller) { $previousInstallerPath = Resolve-File $PreviousInstaller 'previous installer' }
$betaManifestPath = ''
$betaPackagePath = ''
if ($BetaManifest) {
    $betaManifestPath = Resolve-File $BetaManifest 'beta manifest'
    $betaPackagePath = Resolve-File $BetaPackage 'beta package'
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot ('acceptance\release-candidate-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$windowsDir = Join-Path $OutputDirectory 'windows'
$freshReport = Join-Path $OutputDirectory 'fresh-install.json'
$machineReport = Join-Path $OutputDirectory 'fresh-install-machine.json'
$matrixReport = Join-Path $OutputDirectory 'release-matrix.json'
$upgradeReport = Join-Path $OutputDirectory 'upgrade-install.json'
$summaryPath = Join-Path $OutputDirectory 'release-candidate.json'

Write-Host '=== Gate 0/4: signed build-package identity ===' -ForegroundColor Cyan
$signedDist = Get-Content -LiteralPath $signedDistReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($signedDist.schema -ne 'webcom-build-package-v1') { Fail 'signed dist verification report has an unexpected schema' }
if (-not $signedDist.releaseReadyRequired -or -not $signedDist.signedExecutableRequired) { Fail 'signed dist report was not produced under release-ready + signed-executable policy' }
if ($signedDist.authenticodeStatus -ne 'Valid') { Fail "signed dist executable Authenticode status is '$($signedDist.authenticodeStatus)'" }
if ($signedDist.publisherSha256 -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'signed dist publisher fingerprint does not match the Release Candidate publisher pin' }
if ($signedDist.version -ne $ExpectedVersion) { Fail "signed dist version is '$($signedDist.version)', expected $ExpectedVersion" }
if (-not $signedDist.uiHarnessPresent -or [int]$signedDist.sourceManifestEntries -lt 100) { Fail 'signed dist verification is missing UI-harness or source-manifest evidence' }

Write-Host '=== Gate 1/4: clean Windows/MSVC/WebView2 acceptance ===' -ForegroundColor Cyan
$acceptanceArgs = @{
    Configuration = 'Release'
    Architecture = 'x64'
    OutputDirectory = $windowsDir
}
if (-not $SkipMemoryGate) {
    $acceptanceArgs.MeasureMemory = $true
    $acceptanceArgs.MemorySamples = $MemorySamples
    $acceptanceArgs.MemoryWarmupSeconds = $MemoryWarmupSeconds
    $acceptanceArgs.WorkingSetBudgetMiB = $WorkingSetBudgetMiB
    $acceptanceArgs.PrivateCommitBudgetMiB = $PrivateCommitBudgetMiB
}
& (Join-Path $PSScriptRoot 'Test-WindowsAcceptance.ps1') @acceptanceArgs
$windowsReport = Join-Path $windowsDir 'windows-acceptance.json'
$windows = Get-Content -LiteralPath $windowsReport -Raw -Encoding UTF8 | ConvertFrom-Json
if ($windows.schema -ne 'webcom-windows-acceptance-v1') { Fail 'Windows acceptance report has an unexpected schema' }
if ($windows.version -ne $ExpectedVersion) { Fail "Windows acceptance version is '$($windows.version)', expected $ExpectedVersion" }
if ([int]$windows.compilerWarningCount -ne 0) { Fail 'Windows acceptance reported project MSVC warnings' }
if (-not $windows.runtimeSmoke.windowReady -or [int]$windows.runtimeSmoke.webViewProcessCount -le 0) { Fail 'Windows acceptance runtime smoke is incomplete' }
if (-not $SkipMemoryGate) {
    if (-not $windows.memoryReport) { Fail 'Windows acceptance did not record the required memory report' }
    if (-not (Test-Path -LiteralPath $windows.memoryReport -PathType Leaf)) { Fail 'Windows acceptance memory report is missing' }
}

Write-Host '=== Gate 2/4: signed release manifest/package matrix ===' -ForegroundColor Cyan
$matrixArgs = @{
    StableManifest = $stableManifestPath
    StablePackage = $currentInstallerPath
    ExpectedPublisherSha256 = $ExpectedPublisherSha256
    OutputJson = $matrixReport
}
if ($betaManifestPath) {
    $matrixArgs.BetaManifest = $betaManifestPath
    $matrixArgs.BetaPackage = $betaPackagePath
}
& (Join-Path $PSScriptRoot 'Test-ReleaseMatrix.ps1') @matrixArgs
$matrix = Get-Content -LiteralPath $matrixReport -Raw -Encoding UTF8 | ConvertFrom-Json
if ($matrix.schema -ne 'webcom-release-matrix-v2') { Fail 'release-matrix report has an unexpected schema' }
if ($matrix.stable.version -ne $ExpectedVersion) { Fail "stable manifest version is '$($matrix.stable.version)', expected $ExpectedVersion" }

Write-Host '=== Gate 3/4: signed fresh install + runtime + uninstall ===' -ForegroundColor Cyan
$freshArgs = @{
    Installer = $currentInstallerPath
    ExpectedPublisherSha256 = $ExpectedPublisherSha256
    ExpectedVersion = $ExpectedVersion
    OutputJson = $freshReport
}
& (Join-Path $PSScriptRoot 'Test-FreshInstall.ps1') @freshArgs
$fresh = Get-Content -LiteralPath $freshReport -Raw -Encoding UTF8 | ConvertFrom-Json
if ($fresh.schema -ne 'webcom-fresh-install-v2') { Fail 'fresh-install report has an unexpected schema' }
if (-not $fresh.runtimeSmoke.windowReady -or [int]$fresh.runtimeSmoke.webViewProcessCount -le 0) { Fail 'fresh-install runtime smoke is incomplete' }
if (-not $fresh.uninstalled -or -not $fresh.registryCleanupVerified) { Fail 'fresh-install uninstall/registry cleanup gate did not complete' }
if (-not $fresh.installedExecutableSha256) { Fail 'fresh-install report did not record installed WebCom.exe identity' }
if ($fresh.installedExecutableSha256 -ne $signedDist.executable.sha256) { Fail 'installer contains a WebCom.exe that differs from the signed dist executable approved by package verification' }

$machineRef = $null
if ($RunMachineInstall) {
    Write-Host '=== Optional: elevated machine-install gate ===' -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'Test-FreshInstall.ps1') -Installer $currentInstallerPath `
        -ExpectedPublisherSha256 $ExpectedPublisherSha256 -ExpectedVersion $ExpectedVersion `
        -MachineInstall -OutputJson $machineReport
    $machine = Get-Content -LiteralPath $machineReport -Raw -Encoding UTF8 | ConvertFrom-Json
    if (-not $machine.machineInstall -or -not $machine.registryCleanupVerified) { Fail 'machine-install gate did not complete' }
    $machineRef = Report-Ref $machineReport
}

$upgradeStatus = 'not-run'
$upgradeRef = $null
if ($previousInstallerPath) {
    Write-Host '=== Gate 4/4: in-place upgrade + profile-state compatibility ===' -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'Test-UpgradeInstall.ps1') -PreviousInstaller $previousInstallerPath `
        -CurrentInstaller $currentInstallerPath -ExpectedPublisherSha256 $ExpectedPublisherSha256 `
        -ExpectedVersion $ExpectedVersion -OutputJson $upgradeReport
    $upgrade = Get-Content -LiteralPath $upgradeReport -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($upgrade.schema -ne 'webcom-upgrade-install-v1') { Fail 'upgrade-install report has an unexpected schema' }
    if (-not $upgrade.profileStatePreservedByInstaller) { Fail 'upgrade installer changed profile state before browser launch' }
    if (-not $upgrade.previousRuntimeSmoke.windowReady -or -not $upgrade.currentRuntimeSmoke.windowReady) { Fail 'upgrade runtime compatibility smoke did not complete' }
    if (-not $upgrade.registryCleanupVerified) { Fail 'upgrade uninstall/registry cleanup gate did not complete' }
    $upgradeStatus = 'passed'
    $upgradeRef = Report-Ref $upgradeReport
} else {
    $upgradeStatus = 'waived-no-previous-signed-build'
    Write-Host 'Upgrade-install gate explicitly waived: no previous signed WebCom build exists.' -ForegroundColor Yellow
}

$currentInstallerHash = (Get-FileHash -LiteralPath $currentInstallerPath -Algorithm SHA256).Hash.ToLowerInvariant()
$stableManifestHash = (Get-FileHash -LiteralPath $stableManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
$summary = [ordered]@{
    schema = 'webcom-release-candidate-v1'
    generatedAt = (Get-Date).ToString('o')
    version = $ExpectedVersion
    readyForPublicBeta = $true
    memoryGate = [ordered]@{
        required = (-not $SkipMemoryGate)
        workingSetBudgetMiB = $(if ($SkipMemoryGate) { 0 } else { $WorkingSetBudgetMiB })
        privateCommitBudgetMiB = $(if ($SkipMemoryGate) { 0 } else { $PrivateCommitBudgetMiB })
    }
    currentInstaller = [ordered]@{ path = $currentInstallerPath; sha256 = $currentInstallerHash }
    stableManifest = [ordered]@{ path = $stableManifestPath; sha256 = $stableManifestHash }
    gates = [ordered]@{
        signedBuildPackage = [ordered]@{ status = 'passed'; report = (Report-Ref $signedDistReportPath); executableSha256 = $signedDist.executable.sha256 }
        windowsAcceptance = [ordered]@{ status = 'passed'; report = (Report-Ref $windowsReport) }
        releaseMatrix = [ordered]@{ status = 'passed'; report = (Report-Ref $matrixReport) }
        freshInstall = [ordered]@{ status = 'passed'; report = (Report-Ref $freshReport) }
        machineInstall = [ordered]@{ status = $(if ($RunMachineInstall) { 'passed' } else { 'not-run' }); report = $machineRef }
        upgradeInstall = [ordered]@{ status = $upgradeStatus; report = $upgradeRef }
    }
}
$summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host ''
Write-Host 'WebCom Release Candidate gate passed.' -ForegroundColor Green
Write-Host "Version: $ExpectedVersion"
Write-Host "Installer SHA256: $currentInstallerHash"
Write-Host "Memory gate: $(if ($SkipMemoryGate) { 'skipped' } else { 'passed' })"
Write-Host "Upgrade gate: $upgradeStatus"
Write-Host "Release Candidate report: $summaryPath"
