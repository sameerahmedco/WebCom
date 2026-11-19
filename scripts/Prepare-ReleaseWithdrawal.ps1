[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$CurrentPublicReleaseApproval,
    [Parameter(Mandatory=$true)][ValidateSet('WithdrawOnly','RestoreLastKnownGood')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$IncidentId,
    [Parameter(Mandatory=$true)][string]$Reason,
    [Parameter(Mandatory=$true)][string]$StableManifestUrl,
    [string]$PreviousPublicReleaseApproval='',
    [string]$PreviousInstaller='',
    [string]$PreviousStableManifest='',
    [string]$PublishedHealthReport='',
    [string]$OutputDirectory='incident-response'
)

$ErrorActionPreference='Stop'
function Fail([string]$Message){throw "WebCom release-withdrawal preparation failed: $Message"}
function Resolve-Required([string]$Path,[string]$Label){if(-not (Test-Path -LiteralPath $Path -PathType Leaf)){Fail "$Label not found: $Path"};return (Resolve-Path -LiteralPath $Path).Path}
function Sha256([string]$Path){return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function PublicHttps([string]$Value,[string]$Label){
    $uri=$null
    if(-not [Uri]::TryCreate($Value,[UriKind]::Absolute,[ref]$uri) -or $uri.Scheme -ne 'https'){Fail "$Label must be an absolute HTTPS URL"}
    $host=$uri.DnsSafeHost.ToLowerInvariant()
    if(-not $host -or $host -in @('localhost','127.0.0.1','::1','example.com','example.org','example.net') -or $host.EndsWith('.invalid')){Fail "$Label must use a real public HTTPS host"}
    return $uri.AbsoluteUri
}
function Copy-Verified([string]$Source,[string]$Destination,[string]$ExpectedHash=''){
    $resolved=Resolve-Required $Source 'incident evidence item'
    if($ExpectedHash -and (Sha256 $resolved) -ne $ExpectedHash.ToLowerInvariant()){Fail "incident evidence hash mismatch before copy: $resolved"}
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $resolved -Destination $Destination -Force
    $hash=Sha256 $Destination
    if($ExpectedHash -and $hash -ne $ExpectedHash.ToLowerInvariant()){Fail "incident evidence hash changed after copy: $Destination"}
    return [ordered]@{path=$Destination;sha256=$hash;bytes=(Get-Item -LiteralPath $Destination).Length}
}

if([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT){Fail 'release-withdrawal preparation must execute on Windows'}
if($IncidentId -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{2,63}$'){Fail 'IncidentId must be 3-64 characters using letters, numbers, dot, underscore or dash'}
if([string]::IsNullOrWhiteSpace($Reason) -or $Reason.Length -gt 2000){Fail 'Reason must be non-empty and no longer than 2000 characters'}
$manifestEndpoint=PublicHttps $StableManifestUrl 'StableManifestUrl'
$currentApprovalPath=Resolve-Required $CurrentPublicReleaseApproval 'current PUBLIC-RELEASE approval'
$current=Get-Content -LiteralPath $currentApprovalPath -Raw -Encoding UTF8 | ConvertFrom-Json
if($current.schema -ne 'webcom-public-release-v1' -or -not [bool]$current.approvedForPublicRelease -or $current.channel -ne 'stable'){Fail 'current approval is not an approved Stable WebCom release'}

$out=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
$stage=Join-Path $out ("WebCom-$($current.version)-incident-$IncidentId")
if(Test-Path -LiteralPath $stage){Fail "incident directory already exists: $stage"}
New-Item -ItemType Directory -Path $stage -Force | Out-Null

$items=@()
$items += Copy-Verified $currentApprovalPath (Join-Path $stage 'current/PUBLIC-RELEASE.json')
$currentManifest=Resolve-Required ([string]$current.artifacts.stableManifest.path) 'current approved Stable manifest'
$currentInstaller=Resolve-Required ([string]$current.artifacts.installer.path) 'current approved installer'
$items += Copy-Verified $currentManifest (Join-Path $stage 'current/stable.v2.txt') ([string]$current.artifacts.stableManifest.sha256)
$items += Copy-Verified $currentInstaller (Join-Path $stage ('current/' + (Split-Path -Leaf $currentInstaller))) ([string]$current.artifacts.installer.sha256)
if($PublishedHealthReport){
    $healthPath=Resolve-Required $PublishedHealthReport 'published-release health report'
    $health=Get-Content -LiteralPath $healthPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if($health.schema -ne 'webcom-published-release-health-v1' -or $health.version -ne [string]$current.version){Fail 'published health report does not match the current public release'}
    if(([string]$health.approval.sha256).ToLowerInvariant() -ne (Sha256 $currentApprovalPath)){Fail 'published health report refers to a different PUBLIC-RELEASE approval'}
    if(([string]$health.installer.sha256).ToLowerInvariant() -ne ([string]$current.artifacts.installer.sha256).ToLowerInvariant()){Fail 'published health report refers to a different installer'}
    $items += Copy-Verified $healthPath (Join-Path $stage 'current/published-release-health.json')
}

$rollback=$null
$rollbackReportPath=$null
if($Mode -eq 'RestoreLastKnownGood'){
    if(-not $PreviousPublicReleaseApproval){Fail 'RestoreLastKnownGood requires PreviousPublicReleaseApproval'}
    $rollbackReportPath=Join-Path $stage 'rollback/rollback-candidate.json'
    $args=@{CurrentPublicReleaseApproval=$currentApprovalPath;PreviousPublicReleaseApproval=$PreviousPublicReleaseApproval;OutputJson=$rollbackReportPath}
    if($PreviousInstaller){$args.PreviousInstaller=$PreviousInstaller}
    if($PreviousStableManifest){$args.PreviousStableManifest=$PreviousStableManifest}
    & (Join-Path $PSScriptRoot 'Test-RollbackCandidate.ps1') @args
    $rollback=Get-Content -LiteralPath $rollbackReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if($rollback.schema -ne 'webcom-rollback-candidate-v1' -or -not [bool]$rollback.eligible){Fail 'rollback candidate validation did not produce an eligible target'}
    $previousApprovalPath=Resolve-Required ([string]$rollback.rollback.approval.path) 'rollback approval'
    $previousInstallerPath=Resolve-Required ([string]$rollback.rollback.installer.path) 'rollback installer'
    $previousManifestPath=Resolve-Required ([string]$rollback.rollback.stableManifest.path) 'rollback Stable manifest'
    $items += Copy-Verified $previousApprovalPath (Join-Path $stage 'rollback/PUBLIC-RELEASE.json') ([string]$rollback.rollback.approval.sha256)
    $items += Copy-Verified $previousInstallerPath (Join-Path $stage ('rollback/' + (Split-Path -Leaf $previousInstallerPath))) ([string]$rollback.rollback.installer.sha256)
    $items += Copy-Verified $previousManifestPath (Join-Path $stage 'rollback/stable-restore.v2.txt') ([string]$rollback.rollback.stableManifest.sha256)
}

$root=Split-Path -Parent $PSScriptRoot
foreach($doc in @('SECURITY.md','SUPPORT.md','docs\INCIDENT-RESPONSE.md')){
    $source=Join-Path $root $doc
    $items += Copy-Verified $source (Join-Path $stage ('docs/' + (Split-Path -Leaf $source)))
}

$manual=@()
if($Mode -eq 'WithdrawOnly'){
    $manual += "Withdraw or disable the public Stable manifest endpoint: $manifestEndpoint"
    $manual += 'Remove public download/promotional links for the affected release while preserving local evidence.'
    $manual += 'If a replacement binary is required, create a higher-version hotfix through the full Release Candidate -> approval -> publication-health chain.'
}else{
    $manual += "At the public Stable manifest endpoint ($manifestEndpoint), manually restore the byte-identical rollback/stable-restore.v2.txt after verifying its SHA-256."
    $manual += 'Ensure the previous approved installer is still reachable at the HTTPS package URL embedded in that restored manifest.'
    $manual += 'Restore public download links to the last-known-good approved installer only after the hosted files pass the normal published-release health checks for that approval.'
    $manual += 'Do not expect the lower Stable manifest version to downgrade machines already running the affected newer version; use WebCom verified user-triggered rollback where available or issue a higher-version hotfix.'
}

$plan=[ordered]@{
    schema='webcom-release-withdrawal-v1'
    preparedAt=(Get-Date).ToUniversalTime().ToString('o')
    publicationPerformed=$false
    incident=[ordered]@{id=$IncidentId;reason=$Reason}
    mode=$Mode
    stableManifestUrl=$manifestEndpoint
    current=[ordered]@{version=[string]$current.version;approvalSha256=(Sha256 $currentApprovalPath);installerSha256=([string]$current.artifacts.installer.sha256).ToLowerInvariant();manifestSha256=([string]$current.artifacts.stableManifest.sha256).ToLowerInvariant()}
    rollback=$(if($rollback){[ordered]@{version=[string]$rollback.rollback.version;candidateReport='rollback/rollback-candidate.json';installerSha256=[string]$rollback.rollback.installer.sha256;manifestSha256=[string]$rollback.rollback.stableManifest.sha256}}else{$null})
    manualActions=$manual
    limitations=[ordered]@{
        noAutomaticPublication=$true
        noAutomaticUninstall=$true
        lowerVersionManifestCannotDowngradeAlreadyInstalledCurrentRelease=$true
        affectedInstalledClientsNeedVerifiedRollbackOrHigherVersionHotfix=$true
    }
}
$planPath=Join-Path $stage 'RELEASE-WITHDRAWAL.json'
$plan | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $planPath -Encoding UTF8
$planHash=Sha256 $planPath
("$planHash  RELEASE-WITHDRAWAL.json") | Set-Content -LiteralPath (Join-Path $stage 'RELEASE-WITHDRAWAL.json.sha256') -Encoding ASCII

$manifestItems=@()
Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
    $relative=$_.FullName.Substring($stage.Length).TrimStart([char[]]'\/').Replace('\','/')
    $manifestItems += [ordered]@{path=$relative;sha256=(Sha256 $_.FullName);bytes=$_.Length}
}
$evidence=[ordered]@{schema='webcom-incident-evidence-v1';generatedAt=(Get-Date).ToUniversalTime().ToString('o');incidentId=$IncidentId;currentVersion=[string]$current.version;mode=$Mode;items=$manifestItems}
$evidencePath=Join-Path $stage 'INCIDENT-EVIDENCE.json'
$evidence | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $evidencePath -Encoding UTF8
$evidenceHash=Sha256 $evidencePath
("$evidenceHash  INCIDENT-EVIDENCE.json") | Set-Content -LiteralPath (Join-Path $stage 'INCIDENT-EVIDENCE.json.sha256') -Encoding ASCII

New-Item -ItemType Directory -Path $out -Force | Out-Null
$zip=Join-Path $out ("WebCom-$($current.version)-incident-$IncidentId.zip")
if(Test-Path -LiteralPath $zip){Remove-Item -LiteralPath $zip -Force}
Compress-Archive -LiteralPath $stage -DestinationPath $zip -CompressionLevel Optimal
$zipHash=Sha256 $zip
("$zipHash  " + (Split-Path -Leaf $zip)) | Set-Content -LiteralPath ($zip + '.sha256') -Encoding ASCII
Write-Host 'WebCom release-withdrawal package prepared.' -ForegroundColor Green
Write-Host "Mode: $Mode"
Write-Host "Plan: $planPath"
Write-Host "Evidence ZIP: $zip"
Write-Host 'No website, CDN, manifest endpoint, installer, or client was changed automatically.'
