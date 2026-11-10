[CmdletBinding()]
param()

# Opens the official page; it does not download, execute, or elevate an installer.
$downloadPage = 'https://developer.microsoft.com/microsoft-edge/webview2/#download-section'
Write-Host 'Opening Microsoft WebView2 Runtime downloads. Choose the Evergreen Standalone Installer for your Windows architecture.'
Start-Process $downloadPage
