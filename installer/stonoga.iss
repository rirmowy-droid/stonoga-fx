; Instalator Stonoga FX (Inno Setup 6)
#define AppVer GetEnv("APPVER")
#if AppVer == ""
  #define AppVer "0.0.0"
#endif

[Setup]
AppId={{6E2B8F0A-5C1D-4E0B-9B7A-53544F4E4741}
AppName=Stonoga FX
AppVersion={#AppVer}
AppPublisher=Stonoga
AppPublisherURL=https://github.com/rirmowy-droid/stonoga-fx
DefaultDirName={autopf}\Stonoga FX
DefaultGroupName=Stonoga FX
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=StonogaFX-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Stonoga FX
UninstallDisplayIcon={app}\Stonoga FX.exe
LicenseFile=..\LICENSE

[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Skrót do wersji bez DAW na pulpicie"; Flags: unchecked

[Files]
Source: "..\build\StonogaFX_artefacts\Release\VST3\Stonoga FX.vst3\*"; DestDir: "{commoncf64}\VST3\Stonoga FX.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\StonogaFX_artefacts\Release\Standalone\Stonoga FX.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Stonoga FX (bez DAW)"; Filename: "{app}\Stonoga FX.exe"
Name: "{group}\Odinstaluj Stonoga FX"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Stonoga FX"; Filename: "{app}\Stonoga FX.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Stonoga FX.exe"; Description: "Uruchom Stonoga FX (bez DAW)"; Flags: nowait postinstall skipifsilent unchecked
