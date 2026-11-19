[CmdletBinding()]
param(
    [string]$Version='3.0.0',
    [Parameter(Mandatory=$true)][string]$StableManifestUrl,
    [Parameter(Mandatory=$true)][string]$PackageUrl,
    [Parameter(Mandatory=$true)][string]$PublisherSha256,
    [Parameter(Mandatory=$true)][string]$SigningCertificateSha1,
    [Parameter(Mandatory=$true)][string]$TimestampUrl,
    [string]$PreviousInstaller='',
    [switch]$NoPreviousSignedBuild,
    [Parameter(Mandatory=$true)][string]$PublisherDisplayName,
    [Parameter(Mandatory=$true)][string]$ProductWebsite,
    [Parameter(Mandatory=$true)][string]$RepositoryUrl,
    [Parameter(Mandatory=$true)][string]$SupportUrl,
    [Parameter(Mandatory=$true)][string]$SecurityUrl,
    [Parameter(Mandatory=$true)][string]$PrivacyUrl,
    [Parameter(Mandatory=$true)][string]$ReleaseNotesUrl,
    [Parameter(Mandatory=$true)][ValidateSet('OpenSource','Proprietary')][string]$LicenseMode,
    [string]$LicenseFile='',
    [string]$ReleaseNotesFile='',
    [ValidateRange(1,100)][int]$MemorySamples=10,
    [ValidateRange(1,3600)][int]$MemoryWarmupSeconds=5,
    [ValidateRange(1,300)][double]$WorkingSetBudgetMiB=300,
    [ValidateRange(0,1048576)][double]$PrivateCommitBudgetMiB=0,
    [string]$OutputDir='release'
)

$ErrorActionPreference='Stop'
function Fail([string]$Message){throw "WebCom one-command public release failed: $Message"}
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { Fail 'this workflow must execute on Windows' }
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { Fail 'run this workflow from an elevated PowerShell session because public-release sign-off requires the machine-install gate' }
if ($PreviousInstaller -and $NoPreviousSignedBuild) { Fail 'use PreviousInstaller or NoPreviousSignedBuild, not both' }
if (-not $PreviousInstaller -and -not $NoPreviousSignedBuild) { Fail 'supply PreviousInstaller, or explicitly pass -NoPreviousSignedBuild for the first signed release' }
if ($Version -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}$') { Fail 'public Stable version must be a three-part semantic version' }

$root=Split-Path -Parent $PSScriptRoot
$out=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
New-Item -ItemType Directory -Path $out -Force | Out-Null

Write-Host '=== Phase 1/4: build and sign exact Release Candidate ===' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'Build-ReleaseCandidate.ps1') -Version $Version -Channel stable `
    -StableManifestUrl $StableManifestUrl -PackageUrl $PackageUrl -PublisherSha256 $PublisherSha256 `
    -SigningCertificateSha1 $SigningCertificateSha1 -TimestampUrl $TimestampUrl -OutputDir $out
if ($LASTEXITCODE -ne 0) { Fail "candidate build exited with $LASTEXITCODE" }
$installer=Join-Path $out "WebCom-Setup-$Version-x64.exe"
$manifest=Join-Path $out 'stable.v2.txt'
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { Fail 'candidate installer missing after build' }
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { Fail 'Stable manifest missing after build' }

Write-Host '=== Phase 2/4: run mandatory real-Windows Release Candidate gates ===' -ForegroundColor Cyan
$acceptance=Join-Path $out 'acceptance'
$testArgs=@{
    CurrentInstaller=$installer; StableManifest=$manifest; ExpectedPublisherSha256=$PublisherSha256; ExpectedVersion=$Version;
    RunMachineInstall=$true; MemorySamples=$MemorySamples; MemoryWarmupSeconds=$MemoryWarmupSeconds;
    WorkingSetBudgetMiB=$WorkingSetBudgetMiB; PrivateCommitBudgetMiB=$PrivateCommitBudgetMiB; OutputDirectory=$acceptance
}
if ($PreviousInstaller) { $testArgs.PreviousInstaller=$PreviousInstaller } else { $testArgs.NoPreviousSignedBuild=$true }
& (Join-Path $PSScriptRoot 'Test-ReleaseCandidate.ps1') @testArgs
if ($LASTEXITCODE -ne 0) { Fail "Release Candidate gate exited with $LASTEXITCODE" }
$rc=Join-Path $acceptance 'release-candidate.json'
if (-not (Test-Path -LiteralPath $rc -PathType Leaf)) { Fail 'release-candidate.json missing after acceptance' }

Write-Host '=== Phase 3/4: promote exact tested candidate to public-release approval ===' -ForegroundColor Cyan
$approval=Join-Path $out 'PUBLIC-RELEASE.json'
$approveArgs=@{
    ReleaseCandidateReport=$rc; CurrentInstaller=$installer; StableManifest=$manifest; ExpectedPublisherSha256=$PublisherSha256;
    ExpectedVersion=$Version; PublisherDisplayName=$PublisherDisplayName; ProductWebsite=$ProductWebsite; RepositoryUrl=$RepositoryUrl;
    SupportUrl=$SupportUrl; SecurityUrl=$SecurityUrl; PrivacyUrl=$PrivacyUrl; ReleaseNotesUrl=$ReleaseNotesUrl;
    LicenseMode=$LicenseMode; Output=$approval
}
if ($LicenseFile) { $approveArgs.LicenseFile=$LicenseFile }
if ($ReleaseNotesFile) { $approveArgs.ReleaseNotesFile=$ReleaseNotesFile }
& (Join-Path $PSScriptRoot 'Approve-PublicRelease.ps1') @approveArgs
if ($LASTEXITCODE -ne 0) { Fail "public-release approval exited with $LASTEXITCODE" }

Write-Host '=== Phase 4/4: archive immutable public-release evidence ===' -ForegroundColor Cyan
$evidenceOut=Join-Path $out 'evidence'
& (Join-Path $PSScriptRoot 'New-ReleaseEvidenceBundle.ps1') -PublicReleaseApproval $approval -RepositoryRoot $root -OutputDirectory $evidenceOut
if ($LASTEXITCODE -ne 0) { Fail "release-evidence bundling exited with $LASTEXITCODE" }

$evidenceZip=Join-Path $evidenceOut ("WebCom-$Version-public-release-evidence.zip")
if (-not (Test-Path -LiteralPath $evidenceZip -PathType Leaf)) { Fail 'release-evidence ZIP missing after bundling' }
$runReport=Join-Path $out 'PUBLIC-RELEASE-RUN.json'
$run=[ordered]@{
    schema='webcom-public-release-run-v1'; completedAt=(Get-Date).ToUniversalTime().ToString('o'); version=$Version;
    publicationPerformed=$false;
    installer=[ordered]@{ path=$installer; sha256=(Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant() };
    stableManifest=[ordered]@{ path=$manifest; sha256=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant() };
    releaseCandidate=[ordered]@{ path=$rc; sha256=(Get-FileHash -LiteralPath $rc -Algorithm SHA256).Hash.ToLowerInvariant() };
    approval=[ordered]@{ path=$approval; sha256=(Get-FileHash -LiteralPath $approval -Algorithm SHA256).Hash.ToLowerInvariant() };
    evidence=[ordered]@{ path=$evidenceZip; sha256=(Get-FileHash -LiteralPath $evidenceZip -Algorithm SHA256).Hash.ToLowerInvariant() }
}
$run | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $runReport -Encoding UTF8
$runHash=(Get-FileHash -LiteralPath $runReport -Algorithm SHA256).Hash.ToLowerInvariant()
("$runHash  " + (Split-Path -Leaf $runReport)) | Set-Content -LiteralPath ($runReport + '.sha256') -Encoding ASCII

Write-Host ''
Write-Host 'WebCom public-release validation ceremony completed.' -ForegroundColor Green
Write-Host "Approval: $approval"
Write-Host "Evidence: $evidenceZip"
Write-Host "Run report: $runReport"
Write-Host 'No artifact was uploaded. Publish the approved files manually, then run Test-PublishedRelease.ps1 against the live Stable manifest.'
