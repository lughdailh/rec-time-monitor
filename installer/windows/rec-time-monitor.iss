; Inno Setup script for REC Time Monitor (OBS plugin).
;
; UNTESTED: written and validated for *syntax* only (no Windows machine
; available to actually run it — see README.md's "Windows" section). It
; expects the plugin to already be built AND INSTALLED via:
;
;   cmake --preset windows-x64
;   cmake --build --preset windows-x64
;   cmake --install build_x64 --prefix release\RelWithDebInfo --config RelWithDebInfo
;
; The plain build step alone is NOT enough: cmake/windows/helpers.cmake only
; copies data\ files into build_x64\rundir\...\ as a post-build step for local
; run-from-build-tree convenience — it does NOT put the compiled .dll there.
; The bin\64bit\ + data\ layout this script needs only exists after the
; explicit `cmake --install` step above, which is what the project's own CI
; (.github/scripts/Build-Windows.ps1) does too.
;
; Compile with Inno Setup (https://jrsoftware.org/isinfo.php), either via
; the Compiler GUI (open this file) or from a command prompt:
;   iscc rec-time-monitor.iss
; The resulting installer appears in installer\windows\Output\.

#define MyAppName "REC Time Monitor"
#define MyAppVersion "2.0.0"
#define MyAppPublisher "CCS"
#define PluginDirName "rec-time-monitor"
#define BuiltPluginDir "..\..\release\RelWithDebInfo\" + PluginDirName

[Setup]
AppId={{5B1F7C2A-6E3D-4B8A-9C1E-REC-TIME-MONITOR}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
; OBS Studio's third-party plugin scan location on Windows is the
; machine-wide %ProgramData%\obs-studio\plugins\ (Inno Setup's
; {commonappdata}), NOT the per-user %APPDATA%\obs-studio\plugins\
; ({userappdata}) this used to point at — that's why v1's installer put
; the plugin somewhere OBS never looks. ProgramData needs admin rights.
DefaultDirName={commonappdata}\obs-studio\plugins\{#PluginDirName}
DisableProgramGroupPage=yes
DisableDirPage=yes
DisableReadyPage=yes
PrivilegesRequired=admin
OutputBaseFilename=REC-Time-Monitor-Windows-Setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#BuiltPluginDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
WelcomeLabel2=Aquest instal·lador copiarà el plugin REC Time Monitor a la carpeta de plugins d'OBS Studio (%ProgramData%\obs-studio\plugins\{#PluginDirName}). Tanca OBS Studio abans de continuar. Cal ser administrador.
