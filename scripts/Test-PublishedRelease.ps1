[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PublicReleaseApproval,
    [Parameter(Mandatory=$true)][string]$StableManifestUrl,
    [string]$ExpectedVersion = '3.0.0',
    [string]$OutputJson = ''
)

$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom published-release health check failed: $Message" }
function Sha256([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function CertSha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function PublicHttps([string]$Value,[string]$Label) {
    $uri=$null
    if (-not [Uri]::TryCreate($Value,[UriKind]::Absolute,[ref]$uri) -or $uri.Scheme -ne 'https') { Fail "$Label must be an absolute HTTPS URL" }
    $host=$uri.DnsSafeHost.ToLowerInvariant()
    if (-not $host -or $host -in @('localhost','127.0.0.1','::1','example.com','example.org','example.net') -or $host.EndsWith('.invalid')) { Fail "$Label must use a real public HTTPS host" }
    return $uri.AbsoluteUri
}
function Get-ResponseFinalUrl($Response,[string]$Original) {
    if ($Response.BaseResponse -and $Response.BaseResponse.ResponseUri) { return $Response.BaseResponse.ResponseUri.AbsoluteUri }
    if ($Response.BaseResponse -and $Response.BaseResponse.RequestMessage -and $Response.BaseResponse.RequestMessage.RequestUri) { return $Response.BaseResponse.RequestMessage.RequestUri.AbsoluteUri }
    if ($Response.PSObject.Properties.Name -contains 'BaseResponse' -and $Response.BaseResponse) {
        $candidate=[string]$Response.BaseResponse
        if ($candidate -match '^https://') { return $candidate }
    }
    return $Original
}
function Get-Public([string]$Url,[string]$Destination='') {
    $params=@{ Uri=$Url; MaximumRedirection=5; ErrorAction='Stop'; UseBasicParsing=$true }
    if ($Destination) { $params.OutFile=$Destination; $params.PassThru=$true }
    $response=Invoke-WebRequest @params
    $final=Get-ResponseFinalUrl $response $Url
    if ($final -notmatch '^https://') { Fail "request redirected away from HTTPS: $Url -> $final" }
    return [pscustomobject]@{ response=$response; finalUrl=$final }
}
function Test-PublicPage([string]$Url,[string]$Label) {
    $target=PublicHttps $Url $Label
    try {
        $result=Get-Public $target
        $code=[int]$result.response.StatusCode
        if ($code -lt 200 -or $code -ge 400) { Fail "$Label returned HTTP $code" }
        return [ordered]@{ url=$target; finalUrl=$result.finalUrl; status=$code }
    } catch { Fail "$Label is not reachable: $($_.Exception.Message)" }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { Fail 'published-release verification must execute on Windows' }
if (-not (Test-Path -LiteralPath $PublicReleaseApproval -PathType Leaf)) { Fail 'PUBLIC-RELEASE.json not found' }
$approvalPath=(Resolve-Path -LiteralPath $PublicReleaseApproval).Path
$approval=Get-Content -LiteralPath $approvalPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($approval.schema -ne 'webcom-public-release-v1' -or -not [bool]$approval.approvedForPublicRelease) { Fail 'approval record is not approved for public release' }
if ($approval.version -ne $ExpectedVersion -or $approval.channel -ne 'stable') { Fail 'approval version/channel differs from expected Stable release' }
$manifestUrl=PublicHttps $StableManifestUrl 'StableManifestUrl'
$temp=Join-Path $env:TEMP ('WebCom-PublishedRelease-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp -Force | Out-Null
try {
    $manifestPath=Join-Path $temp 'stable.v2.txt'
    $manifestFetch=Get-Public $manifestUrl $manifestPath
    if ((Sha256 $manifestPath) -ne ([string]$approval.artifacts.stableManifest.sha256).ToLowerInvariant()) { Fail 'published Stable manifest differs from PUBLIC-RELEASE approval' }
    $lines=@(Get-Content -LiteralPath $manifestPath -Encoding UTF8)
    if ($lines.Count -lt 2 -or $lines[0] -ne "WEBCOM_RELEASE`t2") { Fail 'published Stable manifest is not WEBCOM_RELEASE v2' }
    $fields=@{}
    foreach ($line in $lines[1..($lines.Count-1)]) {
        if (-not $line) { continue }
        $parts=$line -split "`t",2
        if ($parts.Count -eq 2) { $fields[$parts[0]]=$parts[1] }
    }
    foreach ($required in @('version','channel','url','sha256','size','signature','publisher_sha256','published')) {
        if (-not $fields.ContainsKey($required)) { Fail "published manifest missing field: $required" }
    }
    if ($fields.version -ne $ExpectedVersion -or $fields.channel -ne 'stable') { Fail 'published manifest advertises the wrong Stable version/channel' }
    if ($fields.signature -ne 'authenticode') { Fail 'published manifest does not require Authenticode' }
    if ($fields.publisher_sha256.ToLowerInvariant() -ne ([string]$approval.publisher.certificateSha256).ToLowerInvariant()) { Fail 'published manifest publisher pin differs from approval' }
    $packageUrl=PublicHttps $fields.url 'manifest package URL'
    $installerPath=Join-Path $temp 'WebCom-Setup.exe'
    $packageFetch=Get-Public $packageUrl $installerPath
    $installerHash=Sha256 $installerPath
    if ($installerHash -ne ([string]$approval.artifacts.installer.sha256).ToLowerInvariant()) { Fail 'published installer differs from PUBLIC-RELEASE approval' }
    if ($installerHash -ne $fields.sha256.ToLowerInvariant()) { Fail 'published installer hash differs from published manifest' }
    if ((Get-Item -LiteralPath $installerPath).Length -ne [int64]$fields.size) { Fail 'published installer byte count differs from published manifest' }
    $signature=Get-AuthenticodeSignature -LiteralPath $installerPath
    if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $signature.SignerCertificate) { Fail "published installer Authenticode status is $($signature.Status)" }
    $publisher=CertSha256 $signature.SignerCertificate
    if ($publisher -ne ([string]$approval.publisher.certificateSha256).ToLowerInvariant()) { Fail 'published installer publisher differs from PUBLIC-RELEASE approval' }

    $pages=[ordered]@{}
    foreach ($entry in @(
        @('product',[string]$approval.publicUrls.product),
        @('repository',[string]$approval.publicUrls.repository),
        @('support',[string]$approval.publicUrls.support),
        @('security',[string]$approval.publicUrls.security),
        @('privacy',[string]$approval.publicUrls.privacy),
        @('releaseNotes',[string]$approval.publicUrls.releaseNotes)
    )) { $pages[$entry[0]]=Test-PublicPage $entry[1] $entry[0] }

    if (-not $OutputJson) { $OutputJson=Join-Path (Split-Path -Parent $approvalPath) 'published-release-health.json' }
    $output=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputJson)
    $parent=Split-Path -Parent $output; if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    $report=[ordered]@{
        schema='webcom-published-release-health-v1'; checkedAt=(Get-Date).ToUniversalTime().ToString('o'); version=$ExpectedVersion; healthy=$true;
        approval=[ordered]@{ path=$approvalPath; sha256=(Sha256 $approvalPath) };
        stableManifest=[ordered]@{ url=$manifestUrl; finalUrl=$manifestFetch.finalUrl; sha256=(Sha256 $manifestPath) };
        installer=[ordered]@{ url=$packageUrl; finalUrl=$packageFetch.finalUrl; sha256=$installerHash; bytes=(Get-Item -LiteralPath $installerPath).Length; publisherSha256=$publisher; authenticodeStatus=[string]$signature.Status };
        publicPages=$pages
    }
    $report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $output -Encoding UTF8
    $reportHash=Sha256 $output
    ("$reportHash  " + (Split-Path -Leaf $output)) | Set-Content -LiteralPath ($output + '.sha256') -Encoding ASCII
    Write-Host 'Published WebCom release health check passed.' -ForegroundColor Green
    Write-Host "Report: $output"
    Write-Host "SHA256: $reportHash"
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
