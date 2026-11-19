[CmdletBinding()]
param(
    [string]$Version = '3.0.0',
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
# Canonical entry: Build-ReleaseCandidate.ps1
# Build-PublicBeta.ps1 is retained as a backwards-compatible implementation
# entry point for existing automation. New release documentation uses this name.
& (Join-Path $PSScriptRoot 'Build-PublicBeta.ps1') @PSBoundParameters
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
