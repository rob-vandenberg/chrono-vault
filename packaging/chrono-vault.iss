; SPDX-License-Identifier: AGPL-3.0-only
;
; Chrono Vault, a password manager.
; Copyright (C) 2026 Rob Vandenberg
;
; --- Version ------------------------------------------------------------
; chrono-vault.iss 0.0.4
;
; --- Version history ----------------------------------------------------
; v0.0.4: Initial release, modelled on Chrono Viewer's
;
; What the Windows setup holds (D-010). Everything it needs is handed to it by
; build-release.bat, so no path of any machine is written down here.

#ifndef ChronoVersion
  #error Run this through build-release.bat, which works out the version
#endif

[Setup]
AppId={{6C1E2D5A-8B4F-4E3A-9D7C-2F5B8A1C3E90}
AppName=Chrono Vault
AppVersion={#ChronoVersion}
AppVerName=Chrono Vault {#ChronoVersion}
AppPublisher=Rob Vandenberg
AppPublisherURL=https://github.com/rob-vandenberg/chrono-vault
VersionInfoVersion={#ChronoVersion}
DefaultDirName={autopf}\Chrono Vault
DefaultGroupName=Chrono Vault
DisableProgramGroupPage=yes
LicenseFile={#ProjectRoot}\LICENSE
OutputDir={#OutDir}
OutputBaseFilename=chrono-vault-{#ChronoVersion}-setup
SetupIconFile={#IconFile}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
UninstallDisplayIcon={app}\chrono-vault.exe
PrivilegesRequiredOverridesAllowed=dialog
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Put a shortcut on the desktop"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; The program and everything build-release.bat put beside it: Qt, its QML
; modules, OpenSSL, Microsoft's C++ runtime, the licences (OFL.txt among them).
Source: "{#Staging}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; The program file carries the program's own icon (D-019).
Name: "{group}\Chrono Vault"; Filename: "{app}\chrono-vault.exe"
Name: "{autodesktop}\Chrono Vault"; Filename: "{app}\chrono-vault.exe"; Tasks: desktopicon

[Registry]
; "Start Chrono Vault at startup" (D-019) writes this value from the program
; itself; the setup never makes it, but takes it away when Chrono Vault is
; uninstalled, so nothing tries to start a program that is gone.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "Chrono Vault"; Flags: dontcreatekey uninsdeletevalue

[Run]
Filename: "{app}\chrono-vault.exe"; Description: "Start Chrono Vault"; Flags: nowait postinstall skipifsilent
