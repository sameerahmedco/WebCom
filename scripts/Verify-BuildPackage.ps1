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
function Get-PeSecurityFlags([string]$Path) {
    $stream = [System.IO.File]::Open($Path,[System.IO.FileMode]::Open,[System.IO.FileAccess]::Read,[System.IO.FileShare]::Read)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        if ($stream.Length -lt 0x100) { Fail "PE file is too small for security inspection: $Path" }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or ($peOffset + 0x70) -gt $stream.Length) { Fail "PE header offset is invalid for security inspection: $Path" }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { Fail "PE signature is invalid for security inspection: $Path" }
        [void]$reader.ReadUInt16() # machine
        [void]$reader.ReadUInt16() # sections
        $stream.Position = $peOffset + 24
        $magic = $reader.ReadUInt16()
        if ($magic -ne 0x20B -and $magic -ne 0x10B) { Fail "unsupported PE optional-header magic for security inspection: $Path" }
        $stream.Position = $peOffset + 24 + 0x46
        $dllCharacteristics = $reader.ReadUInt16()
        return [ordered]@{
            dllCharacteristics = ('0x{0:x4}' -f $dllCharacteristics)
            highEntropyVa = (($dllCharacteristics -band 0x0020) -ne 0)
            dynamicBase = (($dllCharacteristics -band 0x0040) -ne 0)
            nxCompat = (($dllCharacteristics -band 0x0100) -ne 0)
            guardCf = (($dllCharacteristics -band 0x4000) -ne 0)
        }
    }
    finally { $stream.Dispose() }
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

$webComSecurity = Get-PeSecurityFlags $webComExe
$uiSecurity = $null
if (Test-Path -LiteralPath $uiTests -PathType Leaf) { $uiSecurity = Get-PeSecurityFlags $uiTests }

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

foreach ($publicDocument in @('PRIVACY.md','SECURITY.md','THIRD-PARTY-NOTICES.md')) {
    Resolve-RequiredFile (Join-Path $dist $publicDocument) "public release document $publicDocument" | Out-Null
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

$dependencyLockPath = Resolve-RequiredFile (Join-Path $root 'dependencies.lock.json') 'dependency lock'
$packagedDependencyLock = Resolve-RequiredFile (Join-Path $dist 'docs\dependencies.lock.json') 'packaged dependency lock'
$dependencyVerificationPath = Resolve-RequiredFile (Join-Path $dist 'docs\DEPENDENCY-VERIFICATION.json') 'packaged dependency verification'
$dependencyLockSha256 = (Get-FileHash -LiteralPath $dependencyLockPath -Algorithm SHA256).Hash.ToLowerInvariant()
$packagedLockSha256 = (Get-FileHash -LiteralPath $packagedDependencyLock -Algorithm SHA256).Hash.ToLowerInvariant()
if ($packagedLockSha256 -ne $dependencyLockSha256) { Fail 'packaged dependency lock differs from the source dependency lock' }
$dependencyLock = Get-Content -LiteralPath $dependencyLockPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($dependencyLock.schema -ne 'webcom-dependencies-v1') { Fail 'dependency lock has an unsupported schema' }
$webViewDependency = @($dependencyLock.dependencies | Where-Object { $_.packageId -eq 'Microsoft.Web.WebView2' })
if ($webViewDependency.Count -ne 1) { Fail 'dependency lock must contain exactly one Microsoft.Web.WebView2 record' }
$webViewDependency = $webViewDependency[0]
$dependencyVerification = Get-Content -LiteralPath $dependencyVerificationPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($dependencyVerification.schema -ne 'webcom-dependency-verification-v1') { Fail 'dependency verification has an unexpected schema' }
if ($dependencyVerification.lockSha256 -ne $dependencyLockSha256) { Fail 'dependency verification was not produced from the current dependency lock' }
if ($dependencyVerification.packageId -ne $webViewDependency.packageId -or $dependencyVerification.version -ne $webViewDependency.version) { Fail 'dependency verification identity/version differs from dependencies.lock.json' }
if ($dependencyVerification.packageUrl -ne $webViewDependency.packageUrl) { Fail 'dependency verification package URL differs from dependencies.lock.json' }

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
    if ($buildInfo -notmatch '(?m)^Verified release dependencies: REQUIRED$') { Fail 'release-ready package must require official dependency verification' }
    if ($buildInfo -notmatch '(?m)^Native security hardening: /GS /guard:cf /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /CETCOMPAT; safe DLL search \+ heap/image-load mitigations at startup\.$') { Fail 'release-ready package does not record the required native security baseline' }
    foreach ($flag in @('highEntropyVa','dynamicBase','nxCompat','guardCf')) {
        if (-not [bool]$webComSecurity[$flag]) { Fail "release-ready WebCom.exe is missing PE security flag: $flag" }
        if ($RequireUiTests -and $uiSecurity -and -not [bool]$uiSecurity[$flag]) { Fail "release-ready WebCom-ui-tests.exe is missing PE security flag: $flag" }
    }
    if (-not [bool]$dependencyVerification.officialHashVerified -or $dependencyVerification.source -ne 'nuget') { Fail 'release-ready package requires WebView2 NuGet SHA512 verification against official catalog metadata' }
    if (-not $dependencyVerification.packageSha512Base64 -or $dependencyVerification.packageSha512Base64 -ne $dependencyVerification.catalogSha512Base64) { Fail 'release-ready WebView2 package SHA512 evidence is incomplete or inconsistent' }
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
    dependencyLockSha256 = $dependencyLockSha256
    dependencyVerification = [ordered]@{ schema=$dependencyVerification.schema; packageId=$dependencyVerification.packageId; version=$dependencyVerification.version; source=$dependencyVerification.source; officialHashVerified=[bool]$dependencyVerification.officialHashVerified; packageSha512Base64=$dependencyVerification.packageSha512Base64; headerSha256=$dependencyVerification.headerSha256; x64LoaderSha256=$dependencyVerification.x64LoaderSha256; arm64LoaderSha256=$dependencyVerification.arm64LoaderSha256 }
    testsRequired = [bool]$RequireTestsPassed
    releaseReadyRequired = [bool]$RequireReleaseReady
    signedExecutableRequired = [bool]$RequireSignedExecutable
    authenticodeStatus = $signatureStatus
    publisherSha256 = $signedPublisher
    uiHarnessRequired = [bool]$RequireUiTests
    uiHarnessPresent = (Test-Path -LiteralPath $uiTests -PathType Leaf)
    executable = [ordered]@{ path=$webComExe; sha256=(Get-FileHash -LiteralPath $webComExe -Algorithm SHA256).Hash.ToLowerInvariant(); bytes=(Get-Item -LiteralPath $webComExe).Length }
    webView2Loader = [ordered]@{ path=$loaderDll; sha256=(Get-FileHash -LiteralPath $loaderDll -Algorithm SHA256).Hash.ToLowerInvariant(); bytes=(Get-Item -LiteralPath $loaderDll).Length }
    peSecurity = [ordered]@{ webCom=$webComSecurity; uiHarness=$uiSecurity }
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
