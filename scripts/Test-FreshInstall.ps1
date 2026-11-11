param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [string]$ExpectedPublisherSha256 = "",
    [switch]$KeepInstall,
    [switch]$MachineInstall
)
$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "WebCom fresh-install QA failed: $Message" }
function CertSha256($Certificate) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Certificate.RawData))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
if (-not (Test-Path -LiteralPath $Installer -PathType Leaf)) { Fail 'installer not found' }
$sig = Get-AuthenticodeSignature -LiteralPath $Installer
if ($sig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $sig.SignerCertificate) { Fail "installer Authenticode status is $($sig.Status)" }
if ($ExpectedPublisherSha256) {
    if ($ExpectedPublisherSha256 -notmatch '^[0-9A-Fa-f]{64}$') { Fail 'expected publisher SHA-256 must be 64 hex characters' }
    $publisher = CertSha256 $sig.SignerCertificate
    if ($publisher -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'installer publisher fingerprint mismatch' }
}
$qaRoot = Join-Path $env:TEMP ("WebCom-FreshInstall-QA-" + [Guid]::NewGuid().ToString('N'))
$installDir = Join-Path $qaRoot 'WebCom'
New-Item -ItemType Directory -Force -Path $qaRoot | Out-Null
try {
    $mode = if ($MachineInstall) { '/ALLUSERS' } else { '/CURRENTUSER' }
    if ($MachineInstall) {
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = New-Object Security.Principal.WindowsPrincipal($identity)
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            Fail 'machine-install QA must run from an elevated PowerShell session'
        }
    }
    $process = Start-Process -FilePath (Resolve-Path -LiteralPath $Installer).Path -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',$mode,"/DIR=$installDir") -Wait -PassThru
    if ($process.ExitCode -ne 0) { Fail "installer exited with code $($process.ExitCode)" }
    foreach ($name in 'WebCom.exe','WebView2Loader.dll','unins000.exe') {
        if (-not (Test-Path -LiteralPath (Join-Path $installDir $name) -PathType Leaf)) { Fail "installed file missing: $name" }
    }
    $exeSig = Get-AuthenticodeSignature -LiteralPath (Join-Path $installDir 'WebCom.exe')
    if ($exeSig.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or -not $exeSig.SignerCertificate) { Fail "installed WebCom.exe signature status is $($exeSig.Status)" }
    if ($ExpectedPublisherSha256) {
        $exePublisher = CertSha256 $exeSig.SignerCertificate
        if ($exePublisher -ne $ExpectedPublisherSha256.ToLowerInvariant()) { Fail 'installed WebCom.exe publisher fingerprint mismatch' }
    }
    Write-Host "Installed executable signature status: $($exeSig.Status)"
    Write-Host "Fresh-install file layout verified: $installDir" -ForegroundColor Green

    $registryRoot = if ($MachineInstall) { 'Registry::HKEY_LOCAL_MACHINE' } else { 'Registry::HKEY_CURRENT_USER' }
    $registeredApps = Join-Path $registryRoot 'Software\RegisteredApplications'
    $capabilities = Join-Path $registryRoot 'Software\WebCom\Capabilities'
    $urlAssociations = Join-Path $capabilities 'UrlAssociations'
    $fileAssociations = Join-Path $capabilities 'FileAssociations'
    $progId = Join-Path $registryRoot 'Software\Classes\WebCom.Url'
    $htmlProgId = Join-Path $registryRoot 'Software\Classes\WebCom.HTML'
    $pdfProgId = Join-Path $registryRoot 'Software\Classes\WebCom.PDF'
    $openCommand = Join-Path $progId 'shell\open\command'
    $htmlOpenCommand = Join-Path $htmlProgId 'shell\open\command'
    $pdfOpenCommand = Join-Path $pdfProgId 'shell\open\command'
    $applicationKey = Join-Path $registryRoot 'Software\Classes\Applications\WebCom.exe'
    $registered = (Get-ItemProperty -LiteralPath $registeredApps -Name 'WebCom' -ErrorAction Stop).WebCom
    if ($registered -ne 'Software\WebCom\Capabilities') { Fail 'RegisteredApplications WebCom capability path is incorrect' }
    $associations = Get-ItemProperty -LiteralPath $urlAssociations -ErrorAction Stop
    if ($associations.http -ne 'WebCom.Url' -or $associations.https -ne 'WebCom.Url') { Fail 'HTTP/HTTPS capability registration is incomplete' }
    $files = Get-ItemProperty -LiteralPath $fileAssociations -ErrorAction Stop
    if ($files.'.html' -ne 'WebCom.HTML' -or $files.'.htm' -ne 'WebCom.HTML' -or $files.'.pdf' -ne 'WebCom.PDF') { Fail 'HTML/PDF capability registration is incomplete' }
    $command = (Get-Item -LiteralPath $openCommand -ErrorAction Stop).GetValue('')
    if (-not $command -or $command -notlike ('*' + (Join-Path $installDir 'WebCom.exe') + '*')) { Fail 'WebCom.Url open command does not target the installed executable' }
    $htmlCommand = (Get-Item -LiteralPath $htmlOpenCommand -ErrorAction Stop).GetValue('')
    $pdfCommand = (Get-Item -LiteralPath $pdfOpenCommand -ErrorAction Stop).GetValue('')
    $installedExe = Join-Path $installDir 'WebCom.exe'
    if (-not $htmlCommand -or $htmlCommand -notlike ('*' + $installedExe + '*')) { Fail 'WebCom.HTML open command does not target the installed executable' }
    if (-not $pdfCommand -or $pdfCommand -notlike ('*' + $installedExe + '*')) { Fail 'WebCom.PDF open command does not target the installed executable' }
    Write-Host "Default Apps registration verified in $registryRoot." -ForegroundColor Green
    if (-not $KeepInstall) {
        $uninstall = Join-Path $installDir 'unins000.exe'
        $un = Start-Process -FilePath $uninstall -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait -PassThru
        if ($un.ExitCode -ne 0) { Fail "uninstaller exited with code $($un.ExitCode)" }
        if ((Get-ItemProperty -LiteralPath $registeredApps -Name 'WebCom' -ErrorAction SilentlyContinue).WebCom) { Fail 'RegisteredApplications WebCom value remained after uninstall' }
        if (Test-Path -LiteralPath $progId) { Fail 'WebCom.Url ProgID remained after uninstall' }
        if (Test-Path -LiteralPath $htmlProgId) { Fail 'WebCom.HTML ProgID remained after uninstall' }
        if (Test-Path -LiteralPath $pdfProgId) { Fail 'WebCom.PDF ProgID remained after uninstall' }
        if (Test-Path -LiteralPath $applicationKey) { Fail 'Applications\WebCom.exe registration remained after uninstall' }
        Write-Host 'Silent uninstall and Default Apps registration cleanup completed.' -ForegroundColor Green
    }
} finally {
    if (-not $KeepInstall) { Remove-Item -LiteralPath $qaRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
