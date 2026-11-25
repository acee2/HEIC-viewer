; HEIC Viewer - installer (Inno Setup 6)

#define AppVersion "1.2.0"

[Setup]
AppId={{B3B9A4E0-6C2D-4F5A-9E1B-0D7C2A1F4E10}
AppName=HEIC Viewer
AppVersion={#AppVersion}
AppPublisher=HEIC Viewer
DefaultDirName={autopf}\HEIC Viewer
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes
OutputDir=..\dist
OutputBaseFilename=HEIC-Viewer-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=HEIC Viewer
UninstallDisplayIcon={sys}\imageres.dll,-72

[Languages]
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"

[Messages]
FinishedLabel=Fatto! Foto HEIC e video .MOV dell'iPhone ora hanno le miniature e si aprono gratis.%n%nUna volta sola per tipo di file: tasto destro > Apri con > Scegli un'altra app > spunta "Usa sempre questa app" e scegli:%n- foto .HEIC: Visualizzatore foto di Windows%n- video .MOV / .MP4: Windows Media Player

[Tasks]
Name: "removems"; Description: "Rimuovi ""Estensioni immagine HEIF"" di Microsoft (senza il codec HEVC a pagamento non funziona e può interferire)"
Name: "thumbs"; Description: "Rigenera subito le miniature (riavvia Esplora file)"

[Files]
Source: "..\build\Release\HeicViewer.dll"; DestDir: "{app}"; Flags: ignoreversion regserver restartreplace uninsrestartdelete

[Registry]
; Tipo di file aperto dal Visualizzatore foto di Windows classico
Root: HKLM; Subkey: "SOFTWARE\Classes\PhotoViewer.FileAssoc.Heic"; ValueType: string; ValueName: ""; ValueData: "Immagine HEIC"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\PhotoViewer.FileAssoc.Heic"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "Immagine HEIC"
Root: HKLM; Subkey: "SOFTWARE\Classes\PhotoViewer.FileAssoc.Heic\DefaultIcon"; ValueType: expandsz; ValueName: ""; ValueData: "%SystemRoot%\System32\imageres.dll,-72"
Root: HKLM; Subkey: "SOFTWARE\Classes\PhotoViewer.FileAssoc.Heic\shell\open"; ValueType: expandsz; ValueName: "MuiVerb"; ValueData: "@%ProgramFiles%\Windows Photo Viewer\photoviewer.dll,-3043"
Root: HKLM; Subkey: "SOFTWARE\Classes\PhotoViewer.FileAssoc.Heic\shell\open\command"; ValueType: expandsz; ValueName: ""; ValueData: "%SystemRoot%\System32\rundll32.exe ""%ProgramFiles%\Windows Photo Viewer\PhotoViewer.dll"", ImageView_Fullscreen %1"
Root: HKLM; Subkey: "SOFTWARE\Classes\PhotoViewer.FileAssoc.Heic\shell\open\DropTarget"; ValueType: string; ValueName: "Clsid"; ValueData: "{{FFE2A43C-56B9-4bf5-9A79-CC6D4285608A}"

; Estensioni -> Visualizzatore foto
Root: HKLM; Subkey: "SOFTWARE\Classes\.heic"; ValueType: string; ValueName: ""; ValueData: "PhotoViewer.FileAssoc.Heic"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.heif"; ValueType: string; ValueName: ""; ValueData: "PhotoViewer.FileAssoc.Heic"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.hif";  ValueType: string; ValueName: ""; ValueData: "PhotoViewer.FileAssoc.Heic"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.heic\OpenWithProgids"; ValueType: string; ValueName: "PhotoViewer.FileAssoc.Heic"; ValueData: ""; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.heif\OpenWithProgids"; ValueType: string; ValueName: "PhotoViewer.FileAssoc.Heic"; ValueData: ""; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\.hif\OpenWithProgids";  ValueType: string; ValueName: "PhotoViewer.FileAssoc.Heic"; ValueData: ""; Flags: uninsdeletevalue

; Fa comparire il Visualizzatore foto in "Apri con" / App predefinite
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows Photo Viewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heic"; ValueData: "PhotoViewer.FileAssoc.Heic"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows Photo Viewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heif"; ValueData: "PhotoViewer.FileAssoc.Heic"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows Photo Viewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".hif";  ValueData: "PhotoViewer.FileAssoc.Heic"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\RegisteredApplications"; ValueType: string; ValueName: "Windows Photo Viewer"; ValueData: "SOFTWARE\Microsoft\Windows Photo Viewer\Capabilities"

; Windows Media Player compare in "Apri con" per i video
Root: HKLM; Subkey: "SOFTWARE\Classes\.mov\OpenWithList\wmplayer.exe"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\.mp4\OpenWithList\wmplayer.exe"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\.m4v\OpenWithList\wmplayer.exe"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\.mkv\OpenWithList\wmplayer.exe"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\.3gp\OpenWithList\wmplayer.exe"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\.mov"; ValueType: string; ValueName: "PerceivedType"; ValueData: "video"; Flags: createvalueifdoesntexist

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""Get-AppxPackage -Name Microsoft.HEIFImageExtension | Remove-AppxPackage"""; StatusMsg: "Rimozione Estensioni immagine HEIF di Microsoft..."; Flags: runhidden runasoriginaluser waituntilterminated; Tasks: removems
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""Stop-Process -Name explorer -Force; Start-Sleep -Seconds 2; Remove-Item -Path ($env:LOCALAPPDATA + '\Microsoft\Windows\Explorer\thumbcache_*.db') -Force -ErrorAction SilentlyContinue; if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {{ Start-Process explorer.exe }"""; StatusMsg: "Rigenerazione miniature..."; Flags: runhidden runasoriginaluser waituntilterminated; Tasks: thumbs

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
  if not FileExists(ExpandConstant('{commonpf64}\Windows Photo Viewer\PhotoViewer.dll')) then
    MsgBox('Non trovo il Visualizzatore foto di Windows su questo PC.' + #13#10 +
           'Le miniature funzioneranno comunque, ma il doppio clic non potrà aprire le foto nel Visualizzatore classico.',
           mbInformation, MB_OK);
end;
