[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PublicReleaseApproval,
    [string]$RepositoryRoot = '',
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom release-evidence bundle failed: $Message" }
function Resolve-Required([string]$Path,[string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "$Label not found: $Path" }
    return (Resolve-Path -LiteralPath $Path).Path
}
function Sha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Add-Evidence([System.Collections.Generic.List[object]]$Items,[string]$Source,[string]$Relative,[string]$ExpectedHash='') {
    $resolved = Resolve-Required $Source $Relative
    $actual = Sha256 $resolved
    if ($ExpectedHash -and $actual -ne $ExpectedHash.ToLowerInvariant()) { Fail "$Relative SHA-256 differs from approval evidence" }
    $Items.Add([pscustomobject]@{ source=$resolved; relative=$Relative; sha256=$actual; bytes=(Get-Item -LiteralPath $resolved).Length })
}

if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path -Parent $PSScriptRoot }
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$approvalPath = Resolve-Required $PublicReleaseApproval 'PUBLIC-RELEASE.json'
$approval = Get-Content -LiteralPath $approvalPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($approval.schema -ne 'webcom-public-release-v1' -or -not [bool]$approval.approvedForPublicRelease) { Fail 'approval is not a WebCom public-release approval record' }
if ($approval.channel -ne 'stable') { Fail 'evidence bundle requires a Stable public-release approval' }
if ($approval.version -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}$') { Fail 'approval version is not a Stable semantic version' }

$approvalDir = Split-Path -Parent $approvalPath
$approvalShaPath = Resolve-Required ($approvalPath + '.sha256') 'PUBLIC-RELEASE.json.sha256'
$publicChecksums = Resolve-Required (Join-Path $approvalDir 'PUBLIC-RELEASE-SHA256SUMS.txt') 'PUBLIC-RELEASE-SHA256SUMS.txt'
$expectedApprovalLine = (Get-Content -LiteralPath $approvalShaPath -Raw -Encoding ASCII).Trim()
$expectedApprovalHash = ($expectedApprovalLine -split '\s+',2)[0].ToLowerInvariant()
if ($expectedApprovalHash -ne (Sha256 $approvalPath)) { Fail 'PUBLIC-RELEASE.json.sha256 does not match the approval record' }

$items = [System.Collections.Generic.List[object]]::new()
Add-Evidence $items $approvalPath 'approval/PUBLIC-RELEASE.json' $expectedApprovalHash
Add-Evidence $items $approvalShaPath 'approval/PUBLIC-RELEASE.json.sha256'
Add-Evidence $items $publicChecksums 'approval/PUBLIC-RELEASE-SHA256SUMS.txt'
Add-Evidence $items ([string]$approval.artifacts.installer.path) ('artifacts/' + (Split-Path -Leaf ([string]$approval.artifacts.installer.path))) ([string]$approval.artifacts.installer.sha256)
Add-Evidence $items ([string]$approval.artifacts.stableManifest.path) 'artifacts/stable.v2.txt' ([string]$approval.artifacts.stableManifest.sha256)
Add-Evidence $items ([string]$approval.artifacts.signedBuildPackage.path) 'evidence/signed-dist-verification.json' ([string]$approval.artifacts.signedBuildPackage.sha256)
Add-Evidence $items ([string]$approval.artifacts.provenance.path) 'evidence/release-provenance.json' ([string]$approval.artifacts.provenance.sha256)
Add-Evidence $items ([string]$approval.artifacts.sbom.path) 'evidence/webcom.spdx.json' ([string]$approval.artifacts.sbom.sha256)
Add-Evidence $items ([string]$approval.artifacts.releaseNotes.path) 'docs/RELEASE-NOTES.md' ([string]$approval.artifacts.releaseNotes.sha256)
Add-Evidence $items ([string]$approval.artifacts.releaseCandidate.path) 'evidence/release-candidate.json' ([string]$approval.artifacts.releaseCandidate.sha256)
Add-Evidence $items ([string]$approval.acceptance.windowsAcceptanceReport.path) 'acceptance/windows-acceptance.json' ([string]$approval.acceptance.windowsAcceptanceReport.sha256)
Add-Evidence $items ([string]$approval.acceptance.freshInstallReport.path) 'acceptance/fresh-install.json' ([string]$approval.acceptance.freshInstallReport.sha256)
Add-Evidence $items ([string]$approval.acceptance.machineInstallReport.path) 'acceptance/fresh-install-machine.json' ([string]$approval.acceptance.machineInstallReport.sha256)
if ($approval.acceptance.upgradeInstallReport) {
    Add-Evidence $items ([string]$approval.acceptance.upgradeInstallReport.path) 'acceptance/upgrade-install.json' ([string]$approval.acceptance.upgradeInstallReport.sha256)
}
Add-Evidence $items (Join-Path $root 'docs\source-hashes.sha256') 'source/source-hashes.sha256'
Add-Evidence $items (Join-Path $root 'dependencies.lock.json') 'source/dependencies.lock.json'
Add-Evidence $items (Join-Path $root 'SECURITY.md') 'docs/SECURITY.md'
Add-Evidence $items (Join-Path $root 'PRIVACY.md') 'docs/PRIVACY.md'
Add-Evidence $items (Join-Path $root 'THIRD-PARTY-NOTICES.md') 'docs/THIRD-PARTY-NOTICES.md'

if (-not $OutputDirectory) { $OutputDirectory = Join-Path $approvalDir 'public-release-evidence' }
$out = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
$stage = Join-Path $out ("WebCom-$($approval.version)-release-evidence")
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
foreach ($item in $items) {
    $destination = Join-Path $stage ([string]$item.relative)
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath ([string]$item.source) -Destination $destination -Force
    if ((Sha256 $destination) -ne ([string]$item.sha256)) { Fail "copied evidence hash changed: $($item.relative)" }
}

$manifest = [ordered]@{
    schema='webcom-release-evidence-bundle-v1'
    generatedAt=(Get-Date).ToUniversalTime().ToString('o')
    version=[string]$approval.version
    channel='stable'
    approvalSha256=$expectedApprovalHash
    items=@($items | ForEach-Object { [ordered]@{ path=$_.relative; sha256=$_.sha256; bytes=$_.bytes } })
}
$manifestPath = Join-Path $stage 'EVIDENCE-MANIFEST.json'
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$manifestHash = Sha256 $manifestPath
("$manifestHash  EVIDENCE-MANIFEST.json") | Set-Content -LiteralPath (Join-Path $stage 'EVIDENCE-MANIFEST.json.sha256') -Encoding ASCII

New-Item -ItemType Directory -Path $out -Force | Out-Null
$zip = Join-Path $out ("WebCom-$($approval.version)-public-release-evidence.zip")
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -LiteralPath $stage -DestinationPath $zip -CompressionLevel Optimal
$zipHash = Sha256 $zip
("$zipHash  " + (Split-Path -Leaf $zip)) | Set-Content -LiteralPath ($zip + '.sha256') -Encoding ASCII

Write-Host 'WebCom public-release evidence bundle created.' -ForegroundColor Green
Write-Host "Evidence ZIP: $zip"
Write-Host "ZIP SHA256:  $zipHash"
Write-Host 'The evidence archive contains the approved installer plus all release/acceptance/source identity records; it does not publish or upload them.'
