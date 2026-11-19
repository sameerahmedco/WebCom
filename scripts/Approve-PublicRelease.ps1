[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ReleaseCandidateReport,
    [Parameter(Mandatory=$true)][string]$CurrentInstaller,
    [Parameter(Mandatory=$true)][string]$StableManifest,
    [Parameter(Mandatory=$true)][string]$ExpectedPublisherSha256,
    [string]$ExpectedVersion = '3.0.0',
    [Parameter(Mandatory=$true)][string]$PublisherDisplayName,
    [Parameter(Mandatory=$true)][string]$ProductWebsite,
    [Parameter(Mandatory=$true)][string]$RepositoryUrl,
    [Parameter(Mandatory=$true)][string]$SupportUrl,
    [Parameter(Mandatory=$true)][string]$SecurityUrl,
    [Parameter(Mandatory=$true)][string]$PrivacyUrl,
    [Parameter(Mandatory=$true)][string]$ReleaseNotesUrl,
    [Parameter(Mandatory=$true)][ValidateSet('OpenSource','Proprietary')][string]$LicenseMode,
    [string]$LicenseFile = '',
    [string]$ReleaseNotesFile = '',
    [string]$Output = '',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom public-release approval failed: $Message" }
function Resolve-Required([string]$Path,[string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "$Label not found: $Path" }
    return (Resolve-Path -LiteralPath $Path).Path
}
function Sha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Cert-Sha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Require-PublicHttps([string]$Value,[string]$Label) {
    $uri = $null
    if (-not [Uri]::TryCreate($Value,[UriKind]::Absolute,[ref]$uri) -or $uri.Scheme -ne 'https') { Fail "$Label must be an absolute HTTPS URL" }
    $host = $uri.DnsSafeHost.ToLowerInvariant()
    if (-not $host -or $host -eq 'localhost' -or $host -eq '127.0.0.1' -or $host -eq '::1' -or
        $host.EndsWith('.invalid') -or $host -eq 'example.com' -or $host -eq 'example.org' -or $host -eq 'example.net') {
        Fail "$Label must identify a real public HTTPS host"
    }
    return $uri.AbsoluteUri
}
function Verify-ReportReference($Reference,[string]$Label) {
    if (-not $Reference) { Fail "$Label reference is missing" }
    $path = Resolve-Required ([string]$Reference.path) $Label
    $actual = Sha256 $path
    if ($actual -ne ([string]$Reference.sha256).ToLowerInvariant()) { Fail "$Label SHA-256 differs from the Release Candidate report" }
    if ($Reference.bytes -and [int64]$Reference.bytes -ne (Get-Item -LiteralPath $path).Length) { Fail "$Label byte count differs from the Release Candidate report" }
    return $path
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { Fail 'final public-release approval must execute on Windows' }
if ($ExpectedPublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'ExpectedPublisherSha256 must be 64 hex characters' }
if ($ExpectedVersion -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}$') { Fail 'public Stable release version must be a three-part semantic version without a prerelease suffix' }
if ([string]::IsNullOrWhiteSpace($PublisherDisplayName)) { Fail 'PublisherDisplayName is required' }

$productWebsiteValue = Require-PublicHttps $ProductWebsite 'ProductWebsite'
$repositoryUrlValue = Require-PublicHttps $RepositoryUrl 'RepositoryUrl'
$supportUrlValue = Require-PublicHttps $SupportUrl 'SupportUrl'
$securityUrlValue = Require-PublicHttps $SecurityUrl 'SecurityUrl'
$privacyUrlValue = Require-PublicHttps $PrivacyUrl 'PrivacyUrl'
$releaseNotesUrlValue = Require-PublicHttps $ReleaseNotesUrl 'ReleaseNotesUrl'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$rcPath = Resolve-Required $ReleaseCandidateReport 'Release Candidate report'
$installerPath = Resolve-Required $CurrentInstaller 'current installer'
$manifestPath = Resolve-Required $StableManifest 'Stable manifest'
if (-not $ReleaseNotesFile) { $ReleaseNotesFile = Join-Path $repositoryRoot 'docs\RELEASE-NOTES.md' }
$releaseNotesPath = Resolve-Required $ReleaseNotesFile 'release notes'

$licenseRecord = [ordered]@{ mode = $LicenseMode; file = $null; sha256 = $null }
if ($LicenseMode -eq 'OpenSource') {
    if (-not $LicenseFile) { Fail 'OpenSource publication requires an explicit LicenseFile' }
    $licensePath = Resolve-Required $LicenseFile 'open-source license file'
    if ((Get-Item -LiteralPath $licensePath).Length -le 0) { Fail 'open-source license file is empty' }
    $licenseRecord.file = $licensePath
    $licenseRecord.sha256 = Sha256 $licensePath
} elseif ($LicenseFile) {
    $licensePath = Resolve-Required $LicenseFile 'proprietary license/terms file'
    $licenseRecord.file = $licensePath
    $licenseRecord.sha256 = Sha256 $licensePath
}

$rc = Get-Content -LiteralPath $rcPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($rc.schema -ne 'webcom-release-candidate-v1') { Fail 'Release Candidate report has an unexpected schema' }
if ($rc.version -ne $ExpectedVersion) { Fail "Release Candidate version is '$($rc.version)', expected $ExpectedVersion" }
if (-not [bool]$rc.readyForPublicReleaseReview) { Fail 'Release Candidate report is not marked ready for public-release review' }
if (-not [bool]$rc.memoryGate.required) { Fail 'public release cannot be approved from a Release Candidate run that skipped the memory gate' }
if ([double]$rc.memoryGate.workingSetBudgetMiB -le 0 -or [double]$rc.memoryGate.workingSetBudgetMiB -gt 300) { Fail 'public release requires a one-tab working-set budget no looser than 300 MiB' }

foreach ($gateName in @('signedBuildPackage','supplyChain','windowsAcceptance','releaseMatrix','freshInstall','machineInstall')) {
    $gate = $rc.gates.$gateName
    if (-not $gate -or $gate.status -ne 'passed') { Fail "Release Candidate gate '$gateName' must be passed for public release" }
}
$upgradeStatus = [string]$rc.gates.upgradeInstall.status
if ($upgradeStatus -ne 'passed' -and $upgradeStatus -ne 'waived-no-previous-signed-build') { Fail "upgradeInstall status '$upgradeStatus' is not acceptable for public release" }

$installerHash = Sha256 $installerPath
$manifestHash = Sha256 $manifestPath
if ($installerHash -ne ([string]$rc.currentInstaller.sha256).ToLowerInvariant()) { Fail 'installer SHA-256 differs from the Release Candidate-approved installer' }
if ($manifestHash -ne ([string]$rc.stableManifest.sha256).ToLowerInvariant()) { Fail 'Stable manifest SHA-256 differs from the Release Candidate-approved manifest' }

$signedDistPath = Verify-ReportReference $rc.gates.signedBuildPackage.report 'signed build-package report'
$provenancePath = Verify-ReportReference $rc.gates.supplyChain.provenance 'release provenance'
$sbomPath = Verify-ReportReference $rc.gates.supplyChain.sbom 'SPDX SBOM'
$windowsReportPath = Verify-ReportReference $rc.gates.windowsAcceptance.report 'Windows acceptance report'
$matrixReportPath = Verify-ReportReference $rc.gates.releaseMatrix.report 'release-matrix report'
$freshReportPath = Verify-ReportReference $rc.gates.freshInstall.report 'fresh-install report'
$machineReportPath = Verify-ReportReference $rc.gates.machineInstall.report 'machine-install report'
$upgradeReportPath = $null
if ($upgradeStatus -eq 'passed') { $upgradeReportPath = Verify-ReportReference $rc.gates.upgradeInstall.report 'upgrade-install report' }

$signature = Get-AuthenticodeSignature -LiteralPath $installerPath
if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $signature.SignerCertificate) { Fail "installer Authenticode status is $($signature.Status)" }
$publisherFingerprint = Cert-Sha256 $signature.SignerCertificate
if ($publisherFingerprint -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'installer signer fingerprint differs from ExpectedPublisherSha256' }

$provenance = Get-Content -LiteralPath $provenancePath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($provenance.schema -ne 'webcom-release-provenance-v1' -or $provenance.version -ne $ExpectedVersion) { Fail 'release provenance identity/version is inconsistent' }
if ($provenance.installer.sha256 -ne $installerHash) { Fail 'provenance installer hash differs from the approved installer' }
if ($provenance.releaseManifest.sha256 -ne $manifestHash) { Fail 'provenance manifest hash differs from the approved Stable manifest' }
if ($provenance.sbom.sha256 -ne (Sha256 $sbomPath)) { Fail 'provenance SBOM hash differs from the approved SBOM' }

$releaseMatrix = Get-Content -LiteralPath $matrixReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($releaseMatrix.schema -ne 'webcom-release-matrix-v2' -or $releaseMatrix.stable.version -ne $ExpectedVersion) { Fail 'release-matrix Stable identity/version is inconsistent' }

if (-not $Output) { $Output = Join-Path (Split-Path -Parent $installerPath) 'PUBLIC-RELEASE.json' }
$outputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Output)
if ((Test-Path -LiteralPath $outputPath) -and -not $Force) { Fail "approval file already exists: $outputPath (use -Force only for an intentional replacement)" }
$outputParent = Split-Path -Parent $outputPath
if ($outputParent) { New-Item -ItemType Directory -Path $outputParent -Force | Out-Null }

$approval = [ordered]@{
    schema = 'webcom-public-release-v1'
    approvedAt = (Get-Date).ToUniversalTime().ToString('o')
    approvedForPublicRelease = $true
    channel = 'stable'
    version = $ExpectedVersion
    publisher = [ordered]@{
        displayName = $PublisherDisplayName
        certificateSha256 = $publisherFingerprint
    }
    publicUrls = [ordered]@{
        product = $productWebsiteValue
        repository = $repositoryUrlValue
        support = $supportUrlValue
        security = $securityUrlValue
        privacy = $privacyUrlValue
        releaseNotes = $releaseNotesUrlValue
    }
    licensing = $licenseRecord
    artifacts = [ordered]@{
        installer = [ordered]@{ path=$installerPath; sha256=$installerHash; bytes=(Get-Item -LiteralPath $installerPath).Length }
        stableManifest = [ordered]@{ path=$manifestPath; sha256=$manifestHash; bytes=(Get-Item -LiteralPath $manifestPath).Length }
        signedBuildPackage = [ordered]@{ path=$signedDistPath; sha256=(Sha256 $signedDistPath) }
        provenance = [ordered]@{ path=$provenancePath; sha256=(Sha256 $provenancePath) }
        sbom = [ordered]@{ path=$sbomPath; sha256=(Sha256 $sbomPath) }
        releaseNotes = [ordered]@{ path=$releaseNotesPath; sha256=(Sha256 $releaseNotesPath) }
        releaseCandidate = [ordered]@{ path=$rcPath; sha256=(Sha256 $rcPath) }
    }
    acceptance = [ordered]@{
        oneTabMemoryGateRequired = [bool]$rc.memoryGate.required
        workingSetBudgetMiB = [double]$rc.memoryGate.workingSetBudgetMiB
        windowsAcceptanceReport = [ordered]@{ path=$windowsReportPath; sha256=(Sha256 $windowsReportPath) }
        freshInstallReport = [ordered]@{ path=$freshReportPath; sha256=(Sha256 $freshReportPath) }
        machineInstallReport = [ordered]@{ path=$machineReportPath; sha256=(Sha256 $machineReportPath) }
        upgradeInstallStatus = $upgradeStatus
        upgradeInstallReport = $(if ($upgradeReportPath) { [ordered]@{ path=$upgradeReportPath; sha256=(Sha256 $upgradeReportPath) } } else { $null })
    }
}
$approval | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $outputPath -Encoding UTF8
$approvalSha = Sha256 $outputPath
$shaPath = $outputPath + '.sha256'
("$approvalSha  " + (Split-Path -Leaf $outputPath)) | Set-Content -LiteralPath $shaPath -Encoding ASCII
$checksumsPath = Join-Path $outputParent 'PUBLIC-RELEASE-SHA256SUMS.txt'
$checksumLines = @(
    "$installerHash  $(Split-Path -Leaf $installerPath)",
    "$manifestHash  $(Split-Path -Leaf $manifestPath)",
    "$(Sha256 $signedDistPath)  $(Split-Path -Leaf $signedDistPath)",
    "$(Sha256 $provenancePath)  $(Split-Path -Leaf $provenancePath)",
    "$(Sha256 $sbomPath)  $(Split-Path -Leaf $sbomPath)",
    "$(Sha256 $rcPath)  $(Split-Path -Leaf $rcPath)",
    "$approvalSha  $(Split-Path -Leaf $outputPath)"
)
$checksumLines | Set-Content -LiteralPath $checksumsPath -Encoding ASCII

Write-Host ''
Write-Host 'WebCom public-release promotion gate passed.' -ForegroundColor Green
Write-Host "Version:       $ExpectedVersion"
Write-Host "Installer:     $installerPath"
Write-Host "Installer SHA: $installerHash"
Write-Host "Approval:      $outputPath"
Write-Host "Approval SHA:  $approvalSha"
Write-Host "Checksums:     $checksumsPath"
Write-Host 'This command records approval evidence only; it does not upload or publish release artifacts.'
