param(
    [string]$Version = '2.0.0',
    [ValidateSet('stable','beta')][string]$Channel = 'stable',
    [Parameter(Mandatory=$true)][string]$StableManifestUrl,
    [string]$BetaManifestUrl = '',
    [Parameter(Mandatory=$true)][string]$PackageUrl,
    [Parameter(Mandatory=$true)][string]$PublisherSha256,
    [Parameter(Mandatory=$true)][string]$SigningCertificateSha1,
    [Parameter(Mandatory=$true)][string]$TimestampUrl,
    [string]$OutputDir = 'release'
)
$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom public-beta build failed: $Message" }
if ($Version -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}(-[A-Za-z0-9.-]{1,24})?$') { Fail 'invalid semantic version' }
if ($Channel -eq 'stable' -and $Version -match '-') { Fail 'stable public-beta build cannot use a prerelease version' }
if ($StableManifestUrl -notmatch '^https://\S+$') { Fail 'StableManifestUrl must be HTTPS' }
if ($BetaManifestUrl -and $BetaManifestUrl -notmatch '^https://\S+$') { Fail 'BetaManifestUrl must be HTTPS' }
if ($PackageUrl -notmatch '^https://\S+$') { Fail 'PackageUrl must be HTTPS' }
if ($PublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'PublisherSha256 must be 64 hex characters' }

$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Build.ps1') -Configuration Release -Architecture x64 `
    -StableManifestUrl $StableManifestUrl -BetaManifestUrl $BetaManifestUrl -PublisherSha256 $PublisherSha256
if ($LASTEXITCODE -ne 0) { Fail "browser build exited with $LASTEXITCODE" }

$dist = Join-Path $root 'dist\WebCom'
& (Join-Path $PSScriptRoot 'Build-SignedInstaller.ps1') -DistDir $dist -Version $Version `
    -SigningCertificateSha1 $SigningCertificateSha1 -ExpectedPublisherSha256 $PublisherSha256 `
    -TimestampUrl $TimestampUrl -OutputDir $OutputDir
if ($LASTEXITCODE -ne 0) { Fail "signed installer build exited with $LASTEXITCODE" }

$installer = Join-Path ([System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDir))) "WebCom-Setup-$Version-x64.exe"
$manifestRelative = Join-Path $OutputDir "$Channel.v2.txt"
$manifest = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $manifestRelative))
& (Join-Path $PSScriptRoot 'New-ReleaseManifest.ps1') -Package $installer -Version $Version -Channel $Channel `
    -Url $PackageUrl -Output $manifestRelative
if ($LASTEXITCODE -ne 0) { Fail "release manifest generation exited with $LASTEXITCODE" }
& (Join-Path $PSScriptRoot 'Verify-UpdatePackage.ps1') -Manifest $manifest -Package $installer
if ($LASTEXITCODE -ne 0) { Fail "release package verification exited with $LASTEXITCODE" }

Write-Host 'WebCom public-beta release artifacts are ready.' -ForegroundColor Green
Write-Host "Installer: $installer"
Write-Host "Manifest:  $manifest"
Write-Host 'Publish the installer and manifest only after the Windows acceptance checklist passes.'
