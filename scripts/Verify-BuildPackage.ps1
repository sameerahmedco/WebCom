[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$DistDir,
    [string]$RepositoryRoot = '',
    [string]$ExpectedVersion = '3.0.0',
    [ValidateSet('x64','ARM64')][string]$ExpectedArchitecture = 'x64',
    [ValidateSet('Release','Debug')][string]$ExpectedConfiguration = 'Release',
    [string]$ExpectedPublisherSha256 = '',
    [switch]$RequireUiTests,
    [switch]$RequireReleaseReady,
    [switch]$RequireSignedExecutable,
    [switch]$RequireTestsPassed,
    [switch]$SkipSourceManifest,
    [string]$OutputJson = ''
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom build-package verification failed: $Message" }
function Resolve-RequiredFile([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Fail "$Label missing: $Path" }
    return (Resolve-Path -LiteralPath $Path).Path
}
function CertSha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Get-PeMachine([string]$Path) {
    $stream = [System.IO.File]::Open($Path,[System.IO.FileMode]::Open,[System.IO.FileAccess]::Read,[System.IO.FileShare]::Read)
    try {
        if ($stream.Length -lt 0x40) { Fail "PE file is too small: $Path" }
        $reader = New-Object System.IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { Fail "file does not start with an MZ header: $Path" }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or ($peOffset + 6) -gt $stream.Length) { Fail "PE header offset is invalid: $Path" }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { Fail "PE signature is invalid: $Path" }
        return $reader.ReadUInt16()
    }
    finally { $stream.Dispose() }
}
function Expected-Machine([string]$Architecture) {
    if ($Architecture -eq 'ARM64') { return [UInt16]0xAA64 }
    return [UInt16]0x8664
}
function Test-Version([string]$Actual, [string]$Expected) {
    $numeric = ($Expected -split '-',2)[0]
    return $Actual -match ('^' + [regex]::Escape($numeric) + '(?:\.0)?$')
}
function Relative-SourcePath([string]$Root, [string]$FullName) {
    $relative = $FullName.Substring($Root.Length).TrimStart([char[]]@([char]92,[char]47)).Replace([char]92,[char]47)
    return './' + $relative
}
function Is-IgnoredSourcePath([string]$Relative) {
    $plain = $Relative.Substring(2)
    if ($plain -eq 'docs/source-hashes.sha256' -or $plain -eq 'build-last.log') { return $true }
    return $plain -match '^(?:build(?:/|-[^/]+/)|dist/|release/|\.deps/|acceptance/|\.git/|\.vs/)'
}
function Verify-SourceManifest([string]$Root) {
    $manifestPath = Join-Path $Root 'docs\source-hashes.sha256'
    Resolve-RequiredFile $manifestPath 'source integrity manifest' | Out-Null
    $records = @{}
    foreach ($line in Get-Content -LiteralPath $manifestPath -Encoding UTF8) {
        if (-not $line.Trim()) { continue }
        if ($line -notmatch '^(?<hash>[0-9a-f]{64})  (?<path>\./.+)$') { Fail "malformed source-manifest line: $line" }
        $relative = $Matches.path.Replace([char]92,[char]47)
        if ($records.ContainsKey($relative)) { Fail "duplicate source-manifest path: $relative" }
        $records[$relative] = $Matches.hash
    }
    if ($records.Count -lt 100) { Fail "source integrity manifest is unexpectedly small ($($records.Count) entries)" }
    foreach ($relative in $records.Keys) {
        $local = $relative.Substring(2).Replace([char]47,[System.IO.Path]::DirectorySeparatorChar)
        $path = Join-Path $Root $local
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Fail "source-manifest file missing: $relative" }
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $records[$relative]) { Fail "source-manifest hash mismatch: $relative" }
    }
    $actualPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($item in Get-ChildItem -LiteralPath $Root -File -Recurse -Force) {
        $relative = Relative-SourcePath $Root $item.FullName
        if (-not (Is-IgnoredSourcePath $relative)) { [void]$actualPaths.Add($relative) }
    }
    foreach ($relative in $actualPaths) {
        if (-not $records.ContainsKey($relative)) { Fail "source file is not covered by docs/source-hashes.sha256: $relative" }
    }
    foreach ($relative in $records.Keys) {
        if (-not $actualPaths.Contains($relative)) { Fail "source manifest contains a path outside the clean source set: $relative" }
    }
    return $records.Count
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { Fail 'this package verifier must execute on Windows' }
if ($ExpectedVersion -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}(?:-[A-Za-z0-9.-]{1,24})?$') { Fail 'ExpectedVersion must be a semantic version' }
if ($ExpectedPublisherSha256 -and $ExpectedPublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'ExpectedPublisherSha256 must be 64 hex characters' }
$dist = (Resolve-Path -LiteralPath $DistDir).Path
if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path -Parent $PSScriptRoot }
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path

$webComExe = Resolve-RequiredFile (Join-Path $dist 'WebCom.exe') 'WebCom executable'
$loaderDll = Resolve-RequiredFile (Join-Path $dist 'WebView2Loader.dll') 'WebView2 loader'
$uiTests = Join-Path $dist 'WebCom-ui-tests.exe'
if ($RequireUiTests) { Resolve-RequiredFile $uiTests 'native UI harness' | Out-Null }

$expectedMachine = Expected-Machine $ExpectedArchitecture
foreach ($binary in @($webComExe,$loaderDll)) {
    $machine = Get-PeMachine $binary
    if ($machine -ne $expectedMachine) { Fail "PE architecture mismatch for $(Split-Path -Leaf $binary): 0x$($machine.ToString('x4')) expected $ExpectedArchitecture" }
}
if (Test-Path -LiteralPath $uiTests -PathType Leaf) {
    $machine = Get-PeMachine $uiTests
    if ($machine -ne $expectedMachine) { Fail "PE architecture mismatch for WebCom-ui-tests.exe: 0x$($machine.ToString('x4')) expected $ExpectedArchitecture" }
}

$versionInfo = (Get-Item -LiteralPath $webComExe).VersionInfo
if (-not (Test-Version $versionInfo.ProductVersion $ExpectedVersion)) { Fail "ProductVersion is '$($versionInfo.ProductVersion)', expected $ExpectedVersion" }
if (-not (Test-Version $versionInfo.FileVersion $ExpectedVersion)) { Fail "FileVersion is '$($versionInfo.FileVersion)', expected $ExpectedVersion" }
if ($versionInfo.ProductName -ne 'WebCom') { Fail "ProductName is '$($versionInfo.ProductName)', expected WebCom" }
if ($versionInfo.FileDescription -ne 'WebCom browser') { Fail "FileDescription is '$($versionInfo.FileDescription)', expected 'WebCom browser'" }
$signatureStatus = 'NotRequired'
$signedPublisher = ''
if ($RequireSignedExecutable) {
    if (-not $ExpectedPublisherSha256) { Fail 'signed-executable verification requires ExpectedPublisherSha256' }
    $signature = Get-AuthenticodeSignature -LiteralPath $webComExe
    $signatureStatus = [string]$signature.Status
    if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $signature.SignerCertificate) { Fail "WebCom.exe Authenticode status is $($signature.Status)" }
    $signedPublisher = CertSha256 $signature.SignerCertificate
    if ($signedPublisher -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'signed WebCom.exe publisher fingerprint does not match ExpectedPublisherSha256' }
}

$blockerRoot = Join-Path $dist 'extensions\webcom-ad-blocker'
$blockerFiles = @('manifest.json','network-rules.json','site-config.js','web-ads.css','youtube-player.js','youtube.css')
foreach ($name in $blockerFiles) { Resolve-RequiredFile (Join-Path $blockerRoot $name) "bundled blocker asset $name" | Out-Null }
$manifest = Get-Content -LiteralPath (Join-Path $blockerRoot 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ([int]$manifest.manifest_version -ne 3) { Fail 'bundled blocker manifest_version must be 3' }
$dangerousExtensionFiles = @(Get-ChildItem -LiteralPath $blockerRoot -File -Recurse | Where-Object { $_.Extension -match '^\.(?:exe|dll|com|scr|msi|ps1|bat|cmd|vbs|jscript)$' })
if ($dangerousExtensionFiles.Count -gt 0) { Fail "bundled blocker contains executable/script payload not allowed by the package policy: $($dangerousExtensionFiles[0].FullName)" }

$bannedPatterns = @('*.pdb','*.ilk','*.obj','*.lib','*.exp','*.idb','*.tlog','*.lastbuildstate','*.pch','*.tmp','*.bak')
foreach ($pattern in $bannedPatterns) {
    $found = Get-ChildItem -LiteralPath $dist -File -Recurse -Filter $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { Fail "compiler/debug artifact leaked into package: $($found.FullName)" }
}

$buildInfoPath = Resolve-RequiredFile (Join-Path $dist 'docs\BUILD-INFO.txt') 'packaged BUILD-INFO'
$buildInfo = Get-Content -LiteralPath $buildInfoPath -Raw -Encoding UTF8
if ($buildInfo -notmatch [regex]::Escape("Target: $ExpectedArchitecture Windows; configuration: $ExpectedConfiguration")) {
    Fail "BUILD-INFO does not identify the expected $ExpectedArchitecture/$ExpectedConfiguration package"
}
if ($RequireTestsPassed -and $buildInfo -notmatch '(?m)^Tests: PASSED:') { Fail 'release package does not record a passed test run in BUILD-INFO' }
if ($ExpectedConfiguration -eq 'Release' -and $buildInfo -match '(?im)configuration:\s*Debug') { Fail 'Debug package cannot pass Release verification' }
if ($RequireReleaseReady) {
    if ($ExpectedConfiguration -ne 'Release') { Fail 'release-ready package verification requires Release configuration' }
    if (-not $RequireUiTests -or -not $RequireTestsPassed) { Fail 'release-ready verification requires both UI harness and passed tests' }
    if ($buildInfo -notmatch '(?m)^Warnings as errors: ON$') { Fail 'release-ready package must have native warnings-as-errors enabled' }
    if ($buildInfo -notmatch '(?m)^Update stable manifest: https://\S+$') { Fail 'release-ready package must embed a compact HTTPS Stable update manifest endpoint' }
    if ($buildInfo -notmatch '(?m)^Publisher SHA256: (?<publisher>[0-9A-Fa-f]{64})$') { Fail 'release-ready package must record the compiled publisher SHA-256 pin' }
    $compiledPublisher = $Matches.publisher.ToLowerInvariant()
    if ($ExpectedPublisherSha256 -and $compiledPublisher -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'compiled publisher pin does not match the signer expected for this release' }
}

$sourceManifestEntries = 0
if (-not $SkipSourceManifest) { $sourceManifestEntries = Verify-SourceManifest $root }

$summary = [ordered]@{
    schema = 'webcom-build-package-v1'
    generatedAt = (Get-Date).ToString('o')
    version = $ExpectedVersion
    architecture = $ExpectedArchitecture
    configuration = $ExpectedConfiguration
    productVersion = $versionInfo.ProductVersion
    fileVersion = $versionInfo.FileVersion
    sourceManifestEntries = $sourceManifestEntries
    testsRequired = [bool]$RequireTestsPassed
    releaseReadyRequired = [bool]$RequireReleaseReady
    signedExecutableRequired = [bool]$RequireSignedExecutable
    authenticodeStatus = $signatureStatus
    publisherSha256 = $signedPublisher
    uiHarnessRequired = [bool]$RequireUiTests
    uiHarnessPresent = (Test-Path -LiteralPath $uiTests -PathType Leaf)
    executable = [ordered]@{ path=$webComExe; sha256=(Get-FileHash -LiteralPath $webComExe -Algorithm SHA256).Hash.ToLowerInvariant(); bytes=(Get-Item -LiteralPath $webComExe).Length }
    webView2Loader = [ordered]@{ path=$loaderDll; sha256=(Get-FileHash -LiteralPath $loaderDll -Algorithm SHA256).Hash.ToLowerInvariant(); bytes=(Get-Item -LiteralPath $loaderDll).Length }
    blocker = [ordered]@{ manifestVersion=[int]$manifest.manifest_version; files=$blockerFiles }
}
if ($summary.uiHarnessPresent) {
    $summary['uiHarness'] = [ordered]@{ path=$uiTests; sha256=(Get-FileHash -LiteralPath $uiTests -Algorithm SHA256).Hash.ToLowerInvariant(); bytes=(Get-Item -LiteralPath $uiTests).Length }
}
if ($OutputJson) {
    $resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputJson)
    $parent = Split-Path -Parent $resolvedOutput
    if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedOutput -Encoding UTF8
    Write-Host "Build-package report: $resolvedOutput"
}
Write-Host "WebCom build package verified: $ExpectedArchitecture $ExpectedConfiguration $ExpectedVersion" -ForegroundColor Green
Write-Host "Source manifest entries: $sourceManifestEntries"
