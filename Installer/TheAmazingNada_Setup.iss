#define MyAppName "The Amazing Nada"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "BoxBap"
#define ProductFolder "..\Distribution\The Amazing Nada by BoxBap"

[Setup]
AppId={{7A08EC3C-54C5-4DB3-9C7C-8D511FB0DA39}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://boxbap.com
DefaultDirName={autopf}\Box Bap\The Amazing Nada
DefaultGroupName={#MyAppName}
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\The Amazing Nada.exe
LicenseFile=TheAmazingNada_Terms.txt
OutputDir=..\Distribution
OutputBaseFilename=The_Amazing_Nada_Installer_v1.0
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin (64-bit)"; Types: full custom
Name: "standalone"; Description: "Standalone Application"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked; Components: standalone

[Files]
Source: "{#ProductFolder}\The Amazing Notta.vst3\*"; DestDir: "{commoncf}\VST3\The Amazing Nada.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#ProductFolder}\The Amazing Notta.exe"; DestDir: "{app}"; DestName: "The Amazing Nada.exe"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autodesktop}\The Amazing Nada"; Filename: "{app}\The Amazing Nada.exe"; Tasks: desktopicon; Components: standalone
Name: "{group}\The Amazing Nada"; Filename: "{app}\The Amazing Nada.exe"; Components: standalone
