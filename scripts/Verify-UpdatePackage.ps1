param(
    [Parameter(Mandatory=$true)][string]$Manifest,
    [Parameter(Mandatory=$true)][string]$Package
)
$ErrorActionPreference = 'Stop'

function Fail([string]$Message) { throw "WebCom update verification failed: $Message" }
function CertSha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

if (-not (Test-Path -LiteralPath $Manifest -PathType Leaf)) { Fail 'manifest not found' }
if (-not (Test-Path -LiteralPath $Package -PathType Leaf)) { Fail 'package not found' }
$lines = Get-Content -LiteralPath $Manifest -Encoding UTF8
if ($lines.Count -lt 2 -or $lines[0] -ne "WEBCOM_RELEASE`t2") { Fail 'manifest must use WEBCOM_RELEASE v2' }
$fields = @{}
foreach ($line in $lines[1..($lines.Count-1)]) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $parts = $line -split "`t",2
    if ($parts.Count -ne 2) { Fail "malformed manifest record: $line" }
    if ($fields.ContainsKey($parts[0])) { Fail "duplicate field: $($parts[0])" }
    $fields[$parts[0]] = $parts[1]
}
$required = 'version','channel','url','sha256','size','signature','publisher_sha256','published'
foreach ($key in $required) { if (-not $fields.ContainsKey($key)) { Fail "missing field: $key" } }
if ($fields.signature -ne 'authenticode') { Fail 'package signature must be authenticode' }
if ($fields.url -notmatch '^https://') { Fail 'package URL must use HTTPS' }
if ($fields.sha256 -notmatch '^[0-9a-fA-F]{64}$') { Fail 'invalid package SHA-256' }
if ($fields.publisher_sha256 -notmatch '^[0-9a-fA-F]{64}$') { Fail 'invalid publisher certificate fingerprint' }
if ($fields.version -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}(-[A-Za-z0-9.-]{1,24})?$') { Fail 'invalid release version' }
if ($fields.channel -notin @('stable','beta')) { Fail 'invalid release channel' }
if ($fields.channel -eq 'stable' -and $fields.version -match '-') { Fail 'stable channel cannot advertise a prerelease version' }

$file = Get-Item -LiteralPath $Package
if ([string]$file.Length -ne [string]$fields.size) { Fail "package size mismatch (expected $($fields.size), got $($file.Length))" }
$hash = (Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash.ToLowerInvariant()
if ($hash -ne $fields.sha256.ToLowerInvariant()) { Fail 'package SHA-256 mismatch' }
$sig = Get-AuthenticodeSignature -LiteralPath $Package
if ($sig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $sig.SignerCertificate) { Fail "Authenticode verification status: $($sig.Status)" }
$publisher = CertSha256 $sig.SignerCertificate
if ($publisher -ne $fields.publisher_sha256.ToLowerInvariant()) { Fail 'publisher certificate SHA-256 mismatch' }
$versionInfo = $file.VersionInfo
if ($versionInfo.ProductName -ne 'WebCom') { Fail "installer ProductName must be WebCom (got '$($versionInfo.ProductName)')" }
if ($versionInfo.ProductVersion -ne $fields.version) { Fail "installer ProductVersion mismatch (expected $($fields.version), got $($versionInfo.ProductVersion))" }

Write-Host "WebCom update package verified." -ForegroundColor Green
Write-Host "Version: $($fields.version)"
Write-Host "Channel: $($fields.channel)"
Write-Host "SHA256: $hash"
Write-Host "Publisher SHA256: $publisher"
