[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$StableManifest,
    [Parameter(Mandatory=$true)][string]$StablePackage,
    [string]$BetaManifest = '',
    [string]$BetaPackage = '',
    [Parameter(Mandatory=$true)][string]$ExpectedPublisherSha256
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom release-matrix QA failed: $Message" }
function Read-Manifest([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "manifest not found: $Path" }
    $lines = Get-Content -LiteralPath $Path -Encoding UTF8
    if ($lines.Count -lt 2 -or $lines[0] -ne "WEBCOM_RELEASE`t2") { Fail "manifest must use WEBCOM_RELEASE v2: $Path" }
    $fields = @{}
    foreach ($line in $lines[1..($lines.Count-1)]) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line -split "`t",2
        if ($parts.Count -ne 2 -or $fields.ContainsKey($parts[0])) { Fail "malformed or duplicate manifest record in $Path" }
        $fields[$parts[0]] = $parts[1]
    }
    foreach ($name in 'version','channel','url','sha256','size','signature','publisher_sha256','published') {
        if (-not $fields.ContainsKey($name)) { Fail "missing $name in $Path" }
    }
    return $fields
}

if ($ExpectedPublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'ExpectedPublisherSha256 must be 64 hex characters' }
$stable = Read-Manifest $StableManifest
if ($stable.channel -ne 'stable') { Fail 'StableManifest must declare channel=stable' }
if ($stable.version -match '-') { Fail 'stable manifest must not publish a prerelease version' }
if ($stable.publisher_sha256.ToLowerInvariant() -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'stable manifest publisher pin mismatch' }
& (Join-Path $PSScriptRoot 'Verify-UpdatePackage.ps1') -Manifest $StableManifest -Package $StablePackage
if ($LASTEXITCODE -ne 0) { Fail "stable package verification exited with $LASTEXITCODE" }

if ([bool]$BetaManifest -xor [bool]$BetaPackage) { Fail 'BetaManifest and BetaPackage must be supplied together' }
if ($BetaManifest) {
    $beta = Read-Manifest $BetaManifest
    if ($beta.channel -ne 'beta') { Fail 'BetaManifest must declare channel=beta' }
    if ($beta.publisher_sha256.ToLowerInvariant() -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'beta manifest publisher pin mismatch' }
    & (Join-Path $PSScriptRoot 'Verify-UpdatePackage.ps1') -Manifest $BetaManifest -Package $BetaPackage
    if ($LASTEXITCODE -ne 0) { Fail "beta package verification exited with $LASTEXITCODE" }
    if ($beta.version -eq $stable.version -and $beta.sha256 -ne $stable.sha256) {
        Fail 'stable and beta manifests advertise the same version with different packages'
    }
    Write-Host "Beta release accepted: $($beta.version)" -ForegroundColor Green
}

Write-Host "Stable release accepted: $($stable.version)" -ForegroundColor Green
Write-Host 'Release-matrix policy passed: channels, signer pin, package hashes/sizes/signatures, and version resources are internally consistent.' -ForegroundColor Green
