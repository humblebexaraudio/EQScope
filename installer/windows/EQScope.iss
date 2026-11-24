; Inno Setup script for the EQScope Windows installer (VST3 + standalone + user guide, x64).
; Build the plug-in first (Release) and make the user guide (python docs\make_user_guide.py), then from
; the project folder run:
;   iscc installer\windows\EQScope.iss
; It creates dist\EQScope-1.1.0-Windows-Setup.exe. The GitHub workflow does all of this automatically.

#define AppName "EQScope"
#define AppVersion "1.1.0"
#define Publisher "Humble Bexar Audio"
#define BuildDir "..\..\build\EQScope_artefacts\Release"

[Setup]
; Same AppId as EQ Probe (EQScope's name before 1.0), so installing EQScope upgrades it in place.
; Keep it the same in every version.
AppId={{6E0B1C7A-5F3D-4C1E-9B7A-2D4E8F1A3C55}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
DefaultDirName={autopf}\{#Publisher}\{#AppName}
DefaultGroupName={#Publisher}
DisableProgramGroupPage=yes
; Install into ...\Humble Bexar Audio\EQScope even when upgrading EQ Probe (which used ...\EQ Probe).
UsePreviousAppDir=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
InfoBeforeFile=before.txt
InfoAfterFile=after.txt
OutputDir=..\..\dist
OutputBaseFilename=EQScope-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\EQScope.exe

[Types]
Name: "full"; Description: "VST3 plug-in, standalone app and user guide"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Common Files\VST3)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full
Name: "guide"; Description: "User guide (PDF)"; Types: full custom; Flags: fixed

[InstallDelete]
; EQ Probe, EQScope's earlier name
Type: filesandordirs; Name: "{commoncf64}\VST3\EQ Probe.vst3"
Type: filesandordirs; Name: "{autopf}\{#Publisher}\EQ Probe"
Type: files; Name: "{autoprograms}\{#Publisher}\EQ Probe.lnk"
Type: files; Name: "{autoprograms}\{#Publisher}\EQ Probe User Guide.lnk"
Type: files; Name: "{autoprograms}\EQ Probe.lnk"

[Files]
Source: "{#BuildDir}\VST3\EQScope.vst3\*"; DestDir: "{commoncf64}\VST3\EQScope.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\EQScope.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\..\docs\EQScope User Guide.pdf"; DestDir: "{app}"; Components: guide; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#Publisher}\{#AppName}"; Filename: "{app}\EQScope.exe"; Components: standalone
Name: "{autoprograms}\{#Publisher}\{#AppName} User Guide"; Filename: "{app}\EQScope User Guide.pdf"; Components: guide
Name: "{autoprograms}\{#Publisher}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\EQScope User Guide.pdf"; Description: "Open the user guide"; Flags: postinstall shellexec skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\EQScope.vst3"
