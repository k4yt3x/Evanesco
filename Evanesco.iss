#define ApplicationVersion "1.2.0"

[Setup]
AppName=Evanesco
AppVersion={#ApplicationVersion}
AppId={{564d966a-2bd9-4a33-b7f9-1c48510b925e}
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={localappdata}\Programs\Evanesco
DefaultGroupName=Evanesco
DisableProgramGroupPage=yes
OutputBaseFilename=EvanescoUserSetup-{#ApplicationVersion}-x64
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayIcon={app}\Evanesco.exe
PrivilegesRequired=lowest

[Tasks]
Name: "desktopicon"; Description: "Create a &Desktop shortcut"; GroupDescription: "Additional icons:"
Name: "startmenuicon"; Description: "Create a &Start Menu shortcut"; GroupDescription: "Additional icons:"

[Files]
Source: "build\Desktop_Qt_6_9_1_llvm_mingw_64_bit-Release\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{userdesktop}\Evanesco"; Filename: "{app}\Evanesco.exe"; Tasks: desktopicon
Name: "{userprograms}\Evanesco"; Filename: "{app}\Evanesco.exe"; Tasks: startmenuicon

[Run]
Filename: "{app}\Evanesco.exe"; Description: "Launch Evanesco"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\Programs\Evanesco"
