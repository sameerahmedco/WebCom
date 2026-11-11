#ifndef AppVersion
  #define AppVersion "2.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\WebCom"
#endif
#ifndef VersionNumeric
  #define VersionNumeric "2.0.0"
#endif
#ifndef OutputDir
  #define OutputDir "..\release"
#endif

[Setup]
AppId={{F4473649-68DF-4A30-A28B-7F51EFB4DB2A}
AppName=WebCom
AppVersion={#AppVersion}
AppVerName=WebCom {#AppVersion}
AppPublisher=WebCom
DefaultDirName={autopf}\WebCom
DefaultGroupName=WebCom
UninstallDisplayIcon={app}\WebCom.exe
OutputDir={#OutputDir}
OutputBaseFilename=WebCom-Setup-{#AppVersion}-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
CloseApplications=yes
RestartApplications=no
DisableProgramGroupPage=yes
ChangesAssociations=yes
SetupLogging=yes
VersionInfoVersion={#VersionNumeric}.0
VersionInfoProductName=WebCom
VersionInfoProductVersion={#AppVersion}

[Files]
Source: "{#SourceDir}\WebCom.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
; Register WebCom as a candidate for Windows Default Apps. The installer does
; not set user defaults; Windows Settings remains the authority for the choice.
Root: HKA; Subkey: "Software\Classes\WebCom.Url"; ValueType: string; ValueName: ""; ValueData: "WebCom Web Link"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\WebCom.Url"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\WebCom.Url\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\WebCom.exe,0"
Root: HKA; Subkey: "Software\Classes\WebCom.Url\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\WebCom.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\WebCom.HTML"; ValueType: string; ValueName: ""; ValueData: "WebCom HTML Document"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\WebCom.HTML\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\WebCom.exe,0"
Root: HKA; Subkey: "Software\Classes\WebCom.HTML\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\WebCom.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\WebCom.PDF"; ValueType: string; ValueName: ""; ValueData: "WebCom PDF Document"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\WebCom.PDF\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\WebCom.exe,0"
Root: HKA; Subkey: "Software\Classes\WebCom.PDF\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\WebCom.exe"" ""%1"""
Root: HKA; Subkey: "Software\WebCom\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "WebCom"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\WebCom\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "WebCom web browser"
Root: HKA; Subkey: "Software\WebCom\Capabilities"; ValueType: string; ValueName: "ApplicationIcon"; ValueData: "{app}\WebCom.exe,0"
Root: HKA; Subkey: "Software\WebCom\Capabilities\UrlAssociations"; ValueType: string; ValueName: "http"; ValueData: "WebCom.Url"
Root: HKA; Subkey: "Software\WebCom\Capabilities\UrlAssociations"; ValueType: string; ValueName: "https"; ValueData: "WebCom.Url"
Root: HKA; Subkey: "Software\WebCom\Capabilities\FileAssociations"; ValueType: string; ValueName: ".html"; ValueData: "WebCom.HTML"
Root: HKA; Subkey: "Software\WebCom\Capabilities\FileAssociations"; ValueType: string; ValueName: ".htm"; ValueData: "WebCom.HTML"
Root: HKA; Subkey: "Software\WebCom\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pdf"; ValueData: "WebCom.PDF"
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "WebCom"; ValueData: "Software\WebCom\Capabilities"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\WebCom.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "WebCom"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\WebCom.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\WebCom.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\WebCom.exe\SupportedTypes"; ValueType: string; ValueName: ".html"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\WebCom.exe\SupportedTypes"; ValueType: string; ValueName: ".htm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\WebCom.exe\SupportedTypes"; ValueType: string; ValueName: ".pdf"; ValueData: ""

[Icons]
Name: "{autoprograms}\WebCom"; Filename: "{app}\WebCom.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\WebCom"; Filename: "{app}\WebCom.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Run]
Filename: "{app}\WebCom.exe"; Description: "Launch WebCom"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
