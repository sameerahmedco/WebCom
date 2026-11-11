param(
    [Parameter(Mandatory=$true)][string]$DistDir,
    [Parameter(Mandatory=$true)][string]$Version,
    [Parameter(Mandatory=$true)][string]$SigningCertificateSha1,
    [Parameter(Mandatory=$true)][string]$ExpectedPublisherSha256,
    [Parameter(Mandatory=$true)][string]$TimestampUrl,
    [string]$OutputDir = "release",
    [string]$InnoCompiler = "",
    [string]$SignTool = ""
)
$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom installer build failed: $Message" }
function CertSha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
if ($Version -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}(-[A-Za-z0-9.-]{1,24})?$') { Fail 'invalid semantic version' }
$VersionNumeric = ($Version -split '-',2)[0]
if ($SigningCertificateSha1 -notmatch '^[0-9A-Fa-f]{40}$') { Fail 'signing certificate SHA-1 selector must be 40 hex characters' }
if ($ExpectedPublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'expected publisher SHA-256 must be 64 hex characters' }
if ($TimestampUrl -notmatch '^https://\S+$') { Fail 'timestamp URL must use HTTPS' }
$dist = (Resolve-Path -LiteralPath $DistDir).Path
foreach ($name in 'WebCom.exe','WebView2Loader.dll') {
    if (-not (Test-Path -LiteralPath (Join-Path $dist $name) -PathType Leaf)) { Fail "missing release file: $name" }
}
if (-not $InnoCompiler) {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
    if (-not $candidates) { Fail 'ISCC.exe not found. Install Inno Setup 6 or pass -InnoCompiler.' }
    $InnoCompiler = $candidates[0]
}
if (-not (Test-Path -LiteralPath $InnoCompiler -PathType Leaf)) { Fail 'Inno Setup compiler not found' }
if (-not $SignTool) {
    $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $found = Get-ChildItem -LiteralPath $kits -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $found) { Fail 'signtool.exe not found. Install the Windows SDK or pass -SignTool.' }
    $SignTool = $found.FullName
}
$out = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDir))
New-Item -ItemType Directory -Force -Path $out | Out-Null
$webcomExe = Join-Path $dist 'WebCom.exe'
& $SignTool sign /fd SHA256 /sha1 $SigningCertificateSha1 /tr $TimestampUrl /td SHA256 $webcomExe
if ($LASTEXITCODE -ne 0) { Fail "signtool could not sign WebCom.exe (exit $LASTEXITCODE)" }
& $SignTool verify /pa /all /v $webcomExe
if ($LASTEXITCODE -ne 0) { Fail "WebCom.exe signature verification failed (exit $LASTEXITCODE)" }
$exeSig = Get-AuthenticodeSignature -LiteralPath $webcomExe
if ($exeSig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $exeSig.SignerCertificate) { Fail "WebCom.exe Authenticode status is $($exeSig.Status)" }
$exePublisher = CertSha256 $exeSig.SignerCertificate
if ($exePublisher -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail "WebCom.exe publisher SHA-256 mismatch: $exePublisher" }
$iss = Join-Path $PSScriptRoot '..\installer\WebCom.iss'
& $InnoCompiler "/DAppVersion=$Version" "/DVersionNumeric=$VersionNumeric" "/DSourceDir=$dist" "/DOutputDir=$out" $iss
if ($LASTEXITCODE -ne 0) { Fail "Inno Setup exited with code $LASTEXITCODE" }
$installer = Join-Path $out "WebCom-Setup-$Version-x64.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { Fail 'installer output was not created' }
& $SignTool sign /fd SHA256 /sha1 $SigningCertificateSha1 /tr $TimestampUrl /td SHA256 $installer
if ($LASTEXITCODE -ne 0) { Fail "signtool sign exited with code $LASTEXITCODE" }
& $SignTool verify /pa /all /v $installer
if ($LASTEXITCODE -ne 0) { Fail "signtool verify exited with code $LASTEXITCODE" }
$sig = Get-AuthenticodeSignature -LiteralPath $installer
if ($sig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $sig.SignerCertificate) { Fail "Authenticode status is $($sig.Status)" }
$publisher = CertSha256 $sig.SignerCertificate
if ($publisher -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail "publisher SHA-256 mismatch: $publisher" }
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "Signed WebCom installer ready: $installer" -ForegroundColor Green
Write-Host "SHA256: $hash"
Write-Host "Publisher SHA256: $publisher"
