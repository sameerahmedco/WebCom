param(
    [Parameter(Mandatory=$true)][string]$Package,
    [Parameter(Mandatory=$true)][string]$Version,
    [ValidateSet('stable','beta')][string]$Channel='stable',
    [Parameter(Mandatory=$true)][string]$Url,
    [string]$Output='release.v2.txt',
    [string]$Published=(Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
)
$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "Cannot create WebCom release manifest: $Message" }
function CertSha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
if (-not (Test-Path -LiteralPath $Package -PathType Leaf)) { Fail 'package not found' }
if ($Version -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}(-[A-Za-z0-9.-]{1,24})?$') { Fail 'invalid semantic version' }
if ($Channel -eq 'stable' -and $Version -match '-') { Fail 'stable channel cannot publish a prerelease version' }
if ($Url -notmatch '^https://\S+$') { Fail 'URL must be compact HTTPS' }
$sig = Get-AuthenticodeSignature -LiteralPath $Package
if ($sig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $sig.SignerCertificate) { Fail "package must already have a valid Authenticode signature (status: $($sig.Status))" }
$file = Get-Item -LiteralPath $Package
$hash = (Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash.ToLowerInvariant()
$publisher = CertSha256 $sig.SignerCertificate
$lines = @(
    "WEBCOM_RELEASE`t2",
    "version`t$Version",
    "channel`t$Channel",
    "url`t$Url",
    "sha256`t$hash",
    "size`t$($file.Length)",
    "signature`tauthenticode",
    "publisher_sha256`t$publisher",
    "published`t$Published"
)
[System.IO.File]::WriteAllLines((Join-Path (Get-Location) $Output),$lines,(New-Object System.Text.UTF8Encoding($false)))
Write-Host "Wrote signed WebCom release manifest: $Output" -ForegroundColor Green
Write-Host "Publisher SHA256: $publisher"
