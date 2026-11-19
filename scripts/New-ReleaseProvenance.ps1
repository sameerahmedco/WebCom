[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [Parameter(Mandatory=$true)][string]$Manifest,
    [Parameter(Mandatory=$true)][string]$SignedDistReport,
    [string]$RepositoryRoot = '',
    [string]$OutputDir = 'release'
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom release provenance failed: $Message" }
function Resolve-Required([string]$Path,[string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "$Label missing: $Path" }
    return (Resolve-Path -LiteralPath $Path).Path
}
function Hex-FromBase64([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return '' }
    try { return ([BitConverter]::ToString([Convert]::FromBase64String($Value))).Replace('-','').ToLowerInvariant() }
    catch { Fail 'dependency package SHA512 is not valid Base64' }
}

if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path -Parent $PSScriptRoot }
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$installerPath = Resolve-Required $Installer 'installer'
$manifestPath = Resolve-Required $Manifest 'release manifest'
$signedDistPath = Resolve-Required $SignedDistReport 'signed dist report'
$sourceManifestPath = Resolve-Required (Join-Path $root 'docs\source-hashes.sha256') 'source integrity manifest'
$dependencyLockPath = Resolve-Required (Join-Path $root 'dependencies.lock.json') 'dependency lock'

$signedDist = Get-Content -LiteralPath $signedDistPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($signedDist.schema -ne 'webcom-build-package-v1') { Fail 'signed dist report has an unexpected schema' }
if (-not $signedDist.releaseReadyRequired -or -not $signedDist.signedExecutableRequired) { Fail 'signed dist report is not release-ready signed evidence' }
if ($signedDist.authenticodeStatus -ne 'Valid') { Fail 'signed dist executable is not Authenticode-valid' }
if (-not [bool]$signedDist.dependencyVerification.officialHashVerified) { Fail 'signed dist report does not contain official dependency hash verification' }

$lock = Get-Content -LiteralPath $dependencyLockPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($lock.schema -ne 'webcom-dependencies-v1') { Fail 'dependency lock has an unsupported schema' }
$webView2 = @($lock.dependencies | Where-Object { $_.packageId -eq 'Microsoft.Web.WebView2' })
if ($webView2.Count -ne 1) { Fail 'dependency lock must contain exactly one Microsoft.Web.WebView2 record' }
$webView2 = $webView2[0]
if ($signedDist.dependencyVerification.packageId -ne $webView2.packageId -or $signedDist.dependencyVerification.version -ne $webView2.version) { Fail 'signed dist dependency identity/version differs from the lock' }

$sourceManifestSha256 = (Get-FileHash -LiteralPath $sourceManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
$dependencyLockSha256 = (Get-FileHash -LiteralPath $dependencyLockPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($signedDist.dependencyLockSha256 -ne $dependencyLockSha256) { Fail 'signed dist dependency-lock hash differs from the current source lock' }
$signedDistSha256 = (Get-FileHash -LiteralPath $signedDistPath -Algorithm SHA256).Hash.ToLowerInvariant()
$installerSha256 = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLowerInvariant()
$manifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
$dependencySha512Hex = Hex-FromBase64 ([string]$signedDist.dependencyVerification.packageSha512Base64)
if ($dependencySha512Hex.Length -ne 128) { Fail 'dependency package SHA512 must be 128 hex characters after decoding' }

$out = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDir))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$sbomPath = Join-Path $out 'webcom.spdx.json'
$provenancePath = Join-Path $out 'release-provenance.json'
$created = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
$namespace = "https://webcom.invalid/spdx/$($signedDist.version)/$installerSha256"
$sbom = [ordered]@{
    spdxVersion = 'SPDX-2.3'
    dataLicense = 'CC0-1.0'
    SPDXID = 'SPDXRef-DOCUMENT'
    name = "WebCom-$($signedDist.version)-Windows-x64"
    documentNamespace = $namespace
    creationInfo = [ordered]@{ created=$created; creators=@('Tool: WebCom New-ReleaseProvenance.ps1') }
    packages = @(
        [ordered]@{
            name='WebCom'; SPDXID='SPDXRef-Package-WebCom'; versionInfo=$signedDist.version; downloadLocation='NOASSERTION'; filesAnalyzed=$false;
            licenseConcluded='NOASSERTION'; licenseDeclared='NOASSERTION'; copyrightText='NOASSERTION';
            checksums=@([ordered]@{ algorithm='SHA256'; checksumValue=$installerSha256 })
        },
        [ordered]@{
            name=$webView2.packageId; SPDXID='SPDXRef-Package-WebView2'; versionInfo=$webView2.version; supplier='Organization: Microsoft';
            downloadLocation=$webView2.packageUrl; filesAnalyzed=$false; licenseConcluded='NOASSERTION'; licenseDeclared='NOASSERTION'; copyrightText='NOASSERTION';
            checksums=@([ordered]@{ algorithm='SHA512'; checksumValue=$dependencySha512Hex })
        }
    )
    relationships = @([ordered]@{ spdxElementId='SPDXRef-Package-WebCom'; relationshipType='DEPENDS_ON'; relatedSpdxElement='SPDXRef-Package-WebView2' })
}
$sbom | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $sbomPath -Encoding UTF8
$sbomSha256 = (Get-FileHash -LiteralPath $sbomPath -Algorithm SHA256).Hash.ToLowerInvariant()

$provenance = [ordered]@{
    schema='webcom-release-provenance-v1'; generatedAt=(Get-Date).ToString('o'); version=$signedDist.version; architecture=$signedDist.architecture;
    source=[ordered]@{ manifest='docs/source-hashes.sha256'; manifestEntries=[int]$signedDist.sourceManifestEntries; manifestSha256=$sourceManifestSha256 };
    dependencyLock=[ordered]@{ file='dependencies.lock.json'; sha256=$dependencyLockSha256 };
    webView2=[ordered]@{ packageId=$webView2.packageId; version=$webView2.version; packageUrl=$webView2.packageUrl; officialHashVerified=[bool]$signedDist.dependencyVerification.officialHashVerified; packageSha512Hex=$dependencySha512Hex };
    signedDist=[ordered]@{ report=(Split-Path -Leaf $signedDistPath); reportSha256=$signedDistSha256; executableSha256=$signedDist.executable.sha256; publisherSha256=$signedDist.publisherSha256 };
    installer=[ordered]@{ file=(Split-Path -Leaf $installerPath); sha256=$installerSha256 };
    releaseManifest=[ordered]@{ file=(Split-Path -Leaf $manifestPath); sha256=$manifestSha256 };
    sbom=[ordered]@{ file=(Split-Path -Leaf $sbomPath); sha256=$sbomSha256; format='SPDX-2.3' }
}
$provenance | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $provenancePath -Encoding UTF8

Write-Host 'WebCom release provenance and SPDX SBOM created.' -ForegroundColor Green
Write-Host "Provenance: $provenancePath"
Write-Host "SBOM:       $sbomPath"
