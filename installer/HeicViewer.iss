; HEIC Viewer 2 - installer (Inno Setup 6)
; Installa l'app e la registra in "Apri con" / "App predefinite"
; SENZA impostarla come predefinita: la scelta resta all'utente.

#define AppVersion "2.2.0"

[Setup]
AppId={{6E1B3C52-8F0A-4D7B-9C1E-2A5D4F7B8C90}
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
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\src\HeicViewer\Assets\app.ico
UninstallDisplayIcon={app}\HeicViewer.exe
UninstallDisplayName=HEIC Viewer
CloseApplications=yes

[Languages]
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"

[Messages]
FinishedLabel=HEIC Viewer è installato. Le foto e i video aperti con HEIC Viewer mostrano la copertina in Esplora file.%n%nNon è stato impostato come app predefinita: sei tu a scegliere. Tasto destro su una foto o un video > Apri con > Scegli un'altra app > HEIC Viewer (spunta "Usa sempre questa app" se vuoi che resti).%n%nOppure da Impostazioni > App > App predefinite > Scegli le app predefinite per tipo di file.

[Tasks]
Name: "desktopicon"; Description: "Crea un'icona sul desktop"; Flags: unchecked
Name: "thumbs"; Description: "Rigenera subito le copertine in Esplora file (riavvia Esplora file)"; Flags: unchecked

[Files]
Source: "..\publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build-native\Release\HeicViewerThumbs.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace uninsrestartdelete

[Icons]
Name: "{autoprograms}\HEIC Viewer"; Filename: "{app}\HeicViewer.exe"
Name: "{autodesktop}\HEIC Viewer"; Filename: "{app}\HeicViewer.exe"; Tasks: desktopicon

[Registry]
; --- Copertine (miniature) in Esplora file: solo per i tipi di file di HEIC Viewer ---
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{{6E1B3C53-8F0A-4D7B-9C1E-2A5D4F7B8C90}"; ValueType: string; ValueName: ""; ValueData: "HEIC Viewer - copertine foto"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{{6E1B3C53-8F0A-4D7B-9C1E-2A5D4F7B8C90}\InprocServer32"; ValueType: string; ValueName: ""; ValueData: "{app}\HeicViewerThumbs.dll"
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{{6E1B3C53-8F0A-4D7B-9C1E-2A5D4F7B8C90}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved"; ValueType: string; ValueName: "{{6E1B3C53-8F0A-4D7B-9C1E-2A5D4F7B8C90}"; ValueData: "HEIC Viewer - copertine foto"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{{6E1B3C54-8F0A-4D7B-9C1E-2A5D4F7B8C90}"; ValueType: string; ValueName: ""; ValueData: "HEIC Viewer - copertine video"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{{6E1B3C54-8F0A-4D7B-9C1E-2A5D4F7B8C90}\InprocServer32"; ValueType: string; ValueName: ""; ValueData: "{app}\HeicViewerThumbs.dll"
Root: HKLM; Subkey: "SOFTWARE\Classes\CLSID\{{6E1B3C54-8F0A-4D7B-9C1E-2A5D4F7B8C90}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved"; ValueType: string; ValueName: "{{6E1B3C54-8F0A-4D7B-9C1E-2A5D4F7B8C90}"; ValueData: "HEIC Viewer - copertine video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Image\ShellEx\{{e357fccd-a995-4576-b01f-234630154e96}"; ValueType: string; ValueName: ""; ValueData: "{{6E1B3C53-8F0A-4D7B-9C1E-2A5D4F7B8C90}"
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Video\ShellEx\{{e357fccd-a995-4576-b01f-234630154e96}"; ValueType: string; ValueName: ""; ValueData: "{{6E1B3C54-8F0A-4D7B-9C1E-2A5D4F7B8C90}"
; "Apri con" a volte collega i file al programma invece che ai suoi tipi di file: copertine anche lì
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\ShellEx\{{e357fccd-a995-4576-b01f-234630154e96}"; ValueType: string; ValueName: ""; ValueData: "{{6E1B3C53-8F0A-4D7B-9C1E-2A5D4F7B8C90}"

; --- Registrazione dell'app ---
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Image"; ValueType: string; ValueName: ""; ValueData: "Foto (HEIC Viewer)"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Image"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "Foto"
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Image\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\HeicViewer.exe"",0"
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Image\shell\open"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "HEIC Viewer"
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Image\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\HeicViewer.exe"" ""%1"""
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Video"; ValueType: string; ValueName: ""; ValueData: "Video (HEIC Viewer)"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Video"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Video\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\HeicViewer.exe"",0"
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Video\shell\open"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "HEIC Viewer"
Root: HKLM; Subkey: "SOFTWARE\Classes\HeicViewer.Video\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\HeicViewer.exe"" ""%1"""
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "HEIC Viewer"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\HeicViewer.exe"",0"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\HeicViewer.exe"" ""%1"""
Root: HKLM; Subkey: "SOFTWARE\HeicViewer"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "HEIC Viewer"
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Visualizzatore di foto (anche HEIC) e lettore video (anche MOV/HEVC)"
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities"; ValueType: string; ValueName: "ApplicationIcon"; ValueData: """{app}\HeicViewer.exe"",0"
Root: HKLM; Subkey: "SOFTWARE\RegisteredApplications"; ValueType: string; ValueName: "HEIC Viewer"; ValueData: "SOFTWARE\HeicViewer\Capabilities"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heic"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".heic"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.heic\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heif"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".heif"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.heif\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".hif"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".hif"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.hif\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avif"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".avif"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.avif\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpg"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".jpg"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.jpg\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpeg"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".jpeg"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.jpeg\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpe"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".jpe"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.jpe\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jfif"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".jfif"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.jfif\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".png"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".png"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.png\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".gif"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".gif"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.gif\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".bmp"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".bmp"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.bmp\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dib"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".dib"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.dib\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tif"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".tif"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.tif\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tiff"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".tiff"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.tiff\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webp"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".webp"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.webp\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ico"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".ico"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.ico\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jxl"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".jxl"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.jxl\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tga"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".tga"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.tga\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".psd"; ValueData: "HeicViewer.Image"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".psd"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.psd\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Image"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp4"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".mp4"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.mp4\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4v"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".m4v"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.m4v\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mov"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".mov"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.mov\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mkv"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".mkv"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.mkv\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avi"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".avi"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.avi\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wmv"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".wmv"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.wmv\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webm"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".webm"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.webm\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3gp"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".3gp"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.3gp\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3g2"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".3g2"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.3g2\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mts"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".mts"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.mts\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2ts"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".m2ts"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.m2ts\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ts"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".ts"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.ts\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flv"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".flv"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.flv\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpg"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".mpg"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.mpg\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpeg"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".mpeg"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.mpeg\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\HeicViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogv"; ValueData: "HeicViewer.Video"
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\HeicViewer.exe\SupportedTypes"; ValueType: string; ValueName: ".ogv"; ValueData: ""
Root: HKLM; Subkey: "SOFTWARE\Classes\.ogv\OpenWithProgids"; ValueType: none; ValueName: "HeicViewer.Video"; Flags: uninsdeletevalue

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""Stop-Process -Name explorer -Force; Start-Sleep -Seconds 2; Remove-Item -Path ($env:LOCALAPPDATA + '\Microsoft\Windows\Explorer\thumbcache_*.db') -Force -ErrorAction SilentlyContinue; if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {{ Start-Process explorer.exe }"""; StatusMsg: "Rigenerazione copertine..."; Flags: runhidden runasoriginaluser waituntilterminated; Tasks: thumbs
Filename: "ms-settings:defaultapps"; Description: "Apri le impostazioni delle app predefinite"; Flags: postinstall shellexec skipifsilent unchecked nowait

[Code]
// Se c'è ancora la vecchia versione 1.x (quella con codec e miniature di sistema),
// la disinstalla in silenzio prima di installare la nuova.
const
  OldUninstallKey = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{B3B9A4E0-6C2D-4F5A-9E1B-0D7C2A1F4E10}_is1';

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Uninstaller: String;
  ResultCode: Integer;
begin
  Result := '';
  if RegQueryStringValue(HKLM, OldUninstallKey, 'UninstallString', Uninstaller) then
  begin
    Uninstaller := RemoveQuotes(Uninstaller);
    if FileExists(Uninstaller) then
      Exec(Uninstaller, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;
