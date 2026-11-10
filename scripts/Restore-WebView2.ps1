[CmdletBinding()]
param(
    [string]$SdkDir
)

$ErrorActionPreference = 'Stop'
$sdkVersion = '1.0.4314.35'
$packageId = 'Microsoft.Web.WebView2'
$packageIdLower = $packageId.ToLowerInvariant()
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $repositoryRoot '.deps'

function Test-SdkLayout([string]$Path) {
    foreach ($relativeFile in @(
        'build\native\include\WebView2.h',
        'build\native\x64\WebView2Loader.dll',
        'build\native\x64\WebView2Loader.dll.lib',
        'build\native\arm64\WebView2Loader.dll',
        'build\native\arm64\WebView2Loader.dll.lib'
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $Path $relativeFile) -PathType Leaf)) {
            throw "Incomplete WebView2 SDK: missing $relativeFile in $Path"
        }
    }
}

function Test-PackageIdentity([string]$Path) {
    $nuspec = Get-ChildItem -LiteralPath $Path -Filter '*.nuspec' -File | Select-Object -First 1
    if (-not $nuspec) {
        throw "Downloaded WebView2 package does not contain a .nuspec file."
    }

    [xml]$document = Get-Content -LiteralPath $nuspec.FullName -Raw
    $idNode = $document.SelectSingleNode("/*[local-name()='package']/*[local-name()='metadata']/*[local-name()='id']")
    $versionNode = $document.SelectSingleNode("/*[local-name()='package']/*[local-name()='metadata']/*[local-name()='version']")
    if (-not $idNode -or -not $versionNode) {
        throw "Downloaded WebView2 package has an unreadable NuGet identity."
    }
    if ($idNode.InnerText -cne $packageId) {
        throw "Unexpected NuGet package id '$($idNode.InnerText)'; expected '$packageId'."
    }
    if ($versionNode.InnerText -cne $sdkVersion) {
        throw "Unexpected WebView2 SDK version '$($versionNode.InnerText)'; expected '$sdkVersion'."
    }
}

function Get-OfficialPackageHash {
    # NuGet's flat-container endpoint serves package bytes; it does not expose a
    # sibling .nupkg.sha512 file. The SHA-512 is published in catalog metadata.
    # Catalog lookup is best-effort because some corporate proxies block the
    # registration/catalog endpoints while still allowing the package download.
    $registrationBases = @(
        'https://api.nuget.org/v3/registration5-gz-semver2',
        'https://api.nuget.org/v3/registration5-semver1'
    )

    foreach ($base in $registrationBases) {
        try {
            $registrationUrl = "$base/$packageIdLower/$sdkVersion.json"
            $registration = Invoke-RestMethod -Uri $registrationUrl -TimeoutSec 30
            $catalogEntryUrl = [string]$registration.catalogEntry
            if ([string]::IsNullOrWhiteSpace($catalogEntryUrl)) { continue }

            $catalog = Invoke-RestMethod -Uri $catalogEntryUrl -TimeoutSec 30
            if ([string]$catalog.packageHashAlgorithm -ne 'SHA512') { continue }
            $hash = [string]$catalog.packageHash
            if (-not [string]::IsNullOrWhiteSpace($hash)) { return $hash.Trim() }
        }
        catch {
            # Try the next official registration resource. Failure here is not
            # fatal; package identity/layout validation still runs below.
        }
    }
    return $null
}

if ($SdkDir) {
    $offlineSdk = (Resolve-Path -LiteralPath $SdkDir).Path
    Test-SdkLayout $offlineSdk
    Write-Host "Using the supplied WebView2 SDK: $offlineSdk"
    Write-Output $offlineSdk
    return
}

New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
$packageName = "microsoft.web.webview2.$sdkVersion.nupkg"
$packagePath = Join-Path $dependencyRoot $packageName
$destination = Join-Path $dependencyRoot "Microsoft.Web.WebView2.$sdkVersion"
$packageUrl = "https://api.nuget.org/v3-flatcontainer/$packageIdLower/$sdkVersion/$packageName"

# Windows PowerShell 5.1 can otherwise negotiate an obsolete TLS version.
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
if (-not (Test-Path -LiteralPath $packagePath)) {
    Write-Host "Downloading Microsoft WebView2 SDK $sdkVersion from NuGet..."
    $downloadToken = [Guid]::NewGuid().ToString('N')
    $temporaryPackage = Join-Path $dependencyRoot "$downloadToken.nupkg"
    try {
        Invoke-WebRequest -Uri $packageUrl -OutFile $temporaryPackage -UseBasicParsing -TimeoutSec 90
        if ((Get-Item -LiteralPath $temporaryPackage).Length -lt 1024) {
            throw 'Downloaded WebView2 NuGet package is unexpectedly small.'
        }
        Move-Item -LiteralPath $temporaryPackage -Destination $packagePath -Force
    }
    finally {
        if (Test-Path -LiteralPath $temporaryPackage) {
            Remove-Item -LiteralPath $temporaryPackage -Force
        }
    }
}

$expectedChecksum = Get-OfficialPackageHash
if ($expectedChecksum) {
    $sha512 = [Security.Cryptography.SHA512]::Create()
    $packageStream = [IO.File]::OpenRead($packagePath)
    try {
        $actualChecksum = [Convert]::ToBase64String($sha512.ComputeHash($packageStream))
    }
    finally {
        $packageStream.Dispose()
        $sha512.Dispose()
    }
    if ($actualChecksum -cne $expectedChecksum) {
        Remove-Item -LiteralPath $packagePath -Force -ErrorAction SilentlyContinue
        throw 'WebView2 package SHA512 does not match NuGet catalog metadata. The cached package was deleted; run build.cmd again.'
    }
    Write-Host 'WebView2 package SHA512 verified against NuGet catalog metadata.'
}
else {
    Write-Warning 'NuGet catalog hash metadata was unavailable. Continuing with HTTPS download plus ZIP, package identity, version, and SDK-layout validation.'
}

# Re-extract on every build so modified/stale extracted SDK files are not reused.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stagingDirectory = "$destination.staging.$([Guid]::NewGuid().ToString('N'))"
$archive = $null
try {
    New-Item -ItemType Directory -Path $stagingDirectory | Out-Null
    $stagingPrefix = [IO.Path]::GetFullPath($stagingDirectory) + [IO.Path]::DirectorySeparatorChar
    try {
        $archive = [IO.Compression.ZipFile]::OpenRead($packagePath)
    }
    catch {
        Remove-Item -LiteralPath $packagePath -Force -ErrorAction SilentlyContinue
        throw 'The cached WebView2 NuGet package is not a valid ZIP archive. It was deleted; run build.cmd again.'
    }

    foreach ($entry in $archive.Entries) {
        $entryDestination = [IO.Path]::GetFullPath((Join-Path $stagingDirectory $entry.FullName))
        if (-not $entryDestination.StartsWith($stagingPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "SDK archive contains an invalid path: $($entry.FullName)"
        }
        if ([string]::IsNullOrEmpty($entry.Name)) {
            [IO.Directory]::CreateDirectory($entryDestination) | Out-Null
        }
        else {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($entryDestination)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $entryDestination, $false)
        }
    }
    $archive.Dispose()
    $archive = $null

    Test-PackageIdentity $stagingDirectory
    Test-SdkLayout $stagingDirectory

    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Recurse -Force
    }
    Move-Item -LiteralPath $stagingDirectory -Destination $destination
}
catch {
    if ($_.Exception.Message -match 'package|archive|NuGet|SDK') {
        # Do not keep a package that failed structural/identity validation.
        if (Test-Path -LiteralPath $packagePath) {
            Remove-Item -LiteralPath $packagePath -Force -ErrorAction SilentlyContinue
        }
    }
    throw
}
finally {
    if ($archive) { $archive.Dispose() }
    if (Test-Path -LiteralPath $stagingDirectory) {
        Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
    }
}

Write-Host 'WebView2 SDK restored and validated.'
Write-Output $destination
