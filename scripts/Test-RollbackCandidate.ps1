[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$CurrentPublicReleaseApproval,
    [Parameter(Mandatory=$true)][string]$PreviousPublicReleaseApproval,
    [string]$PreviousInstaller = '',
    [string]$PreviousStableManifest = '',
    [string]$OutputJson = ''
)

$ErrorActionPreference='Stop'
function Fail([string]$Message){throw "WebCom rollback-candidate validation failed: $Message"}
function Resolve-Required([string]$Path,[string]$Label){
    if(-not (Test-Path -LiteralPath $Path -PathType Leaf)){Fail "$Label not found: $Path"}
    return (Resolve-Path -LiteralPath $Path).Path
}
function Sha256([string]$Path){return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function CertSha256($Certificate){
    $sha=[System.Security.Cryptography.SHA256]::Create()
    try{return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant()}
    finally{$sha.Dispose()}
}
function Parse-Manifest([string]$Path){
    $lines=@(Get-Content -LiteralPath $Path -Encoding UTF8)
    if($lines.Count -lt 2 -or $lines[0] -ne "WEBCOM_RELEASE`t2"){Fail 'previous Stable manifest is not WEBCOM_RELEASE v2'}
    $fields=@{}
    foreach($line in $lines[1..($lines.Count-1)]){
        if(-not $line){continue}
        $parts=$line -split "`t",2
        if($parts.Count -eq 2){$fields[$parts[0]]=$parts[1]}
    }
    foreach($required in @('version','channel','url','sha256','size','signature','publisher_sha256','published')){
        if(-not $fields.ContainsKey($required)){Fail "previous Stable manifest missing field: $required"}
    }
    return $fields
}
function PublicHttps([string]$Value,[string]$Label){
    $uri=$null
    if(-not [Uri]::TryCreate($Value,[UriKind]::Absolute,[ref]$uri) -or $uri.Scheme -ne 'https'){Fail "$Label must be an absolute HTTPS URL"}
    $host=$uri.DnsSafeHost.ToLowerInvariant()
    if(-not $host -or $host -in @('localhost','127.0.0.1','::1','example.com','example.org','example.net') -or $host.EndsWith('.invalid')){Fail "$Label must use a real public HTTPS host"}
    return $uri.AbsoluteUri
}

if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT){Fail 'rollback-candidate validation must execute on Windows'}
$currentApprovalPath=Resolve-Required $CurrentPublicReleaseApproval 'current PUBLIC-RELEASE approval'
$previousApprovalPath=Resolve-Required $PreviousPublicReleaseApproval 'previous PUBLIC-RELEASE approval'
$current=Get-Content -LiteralPath $currentApprovalPath -Raw -Encoding UTF8 | ConvertFrom-Json
$previous=Get-Content -LiteralPath $previousApprovalPath -Raw -Encoding UTF8 | ConvertFrom-Json
if($current.schema -ne 'webcom-public-release-v1' -or -not [bool]$current.approvedForPublicRelease -or $current.channel -ne 'stable'){Fail 'current approval is not an approved Stable WebCom public release'}
if($previous.schema -ne 'webcom-public-release-v1' -or -not [bool]$previous.approvedForPublicRelease -or $previous.channel -ne 'stable'){Fail 'previous approval is not an approved Stable WebCom public release'}
$currentVersion=[Version][string]$current.version
$previousVersion=[Version][string]$previous.version
if($previousVersion -ge $currentVersion){Fail "rollback target $previousVersion must be older than current release $currentVersion"}
$currentPublisher=([string]$current.publisher.certificateSha256).ToLowerInvariant()
$previousPublisher=([string]$previous.publisher.certificateSha256).ToLowerInvariant()
if($currentPublisher -notmatch '^[0-9a-f]{64}$' -or $previousPublisher -notmatch '^[0-9a-f]{64}$'){Fail 'release approvals contain an invalid publisher fingerprint'}
if($previousPublisher -ne $currentPublisher){Fail 'rollback target was approved under a different publisher certificate; use a higher-version hotfix instead of this rollback path'}

if(-not $PreviousInstaller){$PreviousInstaller=[string]$previous.artifacts.installer.path}
if(-not $PreviousStableManifest){$PreviousStableManifest=[string]$previous.artifacts.stableManifest.path}
$installerPath=Resolve-Required $PreviousInstaller 'previous approved installer'
$manifestPath=Resolve-Required $PreviousStableManifest 'previous approved Stable manifest'
$installerHash=Sha256 $installerPath
$manifestHash=Sha256 $manifestPath
if($installerHash -ne ([string]$previous.artifacts.installer.sha256).ToLowerInvariant()){Fail 'previous installer SHA-256 differs from its PUBLIC-RELEASE approval'}
if($manifestHash -ne ([string]$previous.artifacts.stableManifest.sha256).ToLowerInvariant()){Fail 'previous Stable manifest SHA-256 differs from its PUBLIC-RELEASE approval'}
if($previous.artifacts.installer.bytes -and [int64]$previous.artifacts.installer.bytes -ne (Get-Item -LiteralPath $installerPath).Length){Fail 'previous installer byte count differs from its PUBLIC-RELEASE approval'}

$manifest=Parse-Manifest $manifestPath
if($manifest.version -ne [string]$previous.version -or $manifest.channel -ne 'stable'){Fail 'previous manifest version/channel differs from the approved rollback target'}
if($manifest.sha256.ToLowerInvariant() -ne $installerHash){Fail 'previous manifest package SHA-256 differs from the approved installer'}
if([int64]$manifest.size -ne (Get-Item -LiteralPath $installerPath).Length){Fail 'previous manifest package size differs from the approved installer'}
if($manifest.signature -ne 'authenticode'){Fail 'previous manifest does not require Authenticode'}
if($manifest.publisher_sha256.ToLowerInvariant() -ne $previousPublisher){Fail 'previous manifest publisher pin differs from the approved rollback publisher'}
$packageUrl=PublicHttps $manifest.url 'previous manifest package URL'

$signature=Get-AuthenticodeSignature -LiteralPath $installerPath
if($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $signature.SignerCertificate){Fail "previous installer Authenticode status is $($signature.Status)"}
$signer=CertSha256 $signature.SignerCertificate
if($signer -ne $previousPublisher){Fail 'previous installer signer differs from the approved rollback publisher'}

if(-not $OutputJson){$OutputJson=Join-Path (Split-Path -Parent $currentApprovalPath) 'rollback-candidate.json'}
$output=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputJson)
$parent=Split-Path -Parent $output;if($parent){New-Item -ItemType Directory -Path $parent -Force | Out-Null}
$report=[ordered]@{
    schema='webcom-rollback-candidate-v1'
    checkedAt=(Get-Date).ToUniversalTime().ToString('o')
    eligible=$true
    current=[ordered]@{version=[string]$current.version;approval=[ordered]@{path=$currentApprovalPath;sha256=(Sha256 $currentApprovalPath)}}
    rollback=[ordered]@{
        version=[string]$previous.version
        approval=[ordered]@{path=$previousApprovalPath;sha256=(Sha256 $previousApprovalPath)}
        installer=[ordered]@{path=$installerPath;sha256=$installerHash;bytes=(Get-Item -LiteralPath $installerPath).Length;publisherSha256=$signer;authenticodeStatus=[string]$signature.Status}
        stableManifest=[ordered]@{path=$manifestPath;sha256=$manifestHash;packageUrl=$packageUrl}
    }
    policy=[ordered]@{
        samePublisherRequired=$true
        canRestoreNewDownloads=$true
        canDowngradeAlreadyInstalledCurrentVersion=$false
        alreadyInstalledCurrentVersionAction='Use WebCom verified user-triggered rollback when available, or publish a higher-version hotfix through the full RC -> approval -> publication chain.'
    }
}
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $output -Encoding UTF8
$hash=Sha256 $output
("$hash  " + (Split-Path -Leaf $output)) | Set-Content -LiteralPath ($output + '.sha256') -Encoding ASCII
Write-Host 'WebCom rollback candidate is valid.' -ForegroundColor Green
Write-Host "Rollback target: $($previous.version)"
Write-Host "Report: $output"
Write-Host 'This validation does not publish, uninstall, or downgrade WebCom.'
