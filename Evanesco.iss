#define ApplicationVersion "1.4.0"

[Setup]
AppName=Evanesco
AppVersion={#ApplicationVersion}
AppId={{564d966a-2bd9-4a33-b7f9-1c48510b925e}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={localappdata}\Programs\Evanesco
DefaultGroupName=Evanesco
DisableProgramGroupPage=yes
OutputBaseFilename=EvanescoUserSetup-{#ApplicationVersion}-x64
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayIcon={app}\Evanesco.exe
PrivilegesRequired=lowest
LicenseFile=LICENSE

[Tasks]
Name: "desktopicon"; Description: "Create a &Desktop shortcut"; GroupDescription: "Additional icons:"
Name: "startmenuicon"; Description: "Create a &Start Menu shortcut"; GroupDescription: "Additional icons:"

[InstallDelete]
Type: filesandordirs; Name: "{app}\*"; Check: IsUpgrade

[Files]
Source: "build\deploy\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{userdesktop}\Evanesco"; Filename: "{app}\Evanesco.exe"; Tasks: desktopicon
Name: "{userprograms}\Evanesco"; Filename: "{app}\Evanesco.exe"; Tasks: startmenuicon

[Run]
Filename: "{app}\Evanesco.exe"; Description: "Launch Evanesco"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{appdata}\Evanesco"

[Code]
function IsUpgrade: Boolean;
begin
  Result := RegKeyExists(HKEY_CURRENT_USER, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{564d966a-2bd9-4a33-b7f9-1c48510b925e}_is1');
end;
