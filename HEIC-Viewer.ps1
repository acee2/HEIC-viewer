<#
  HEIC Viewer per Windows
  -----------------------
  Fa aprire i file .heic / .heif / .hif (foto iPhone) al classico
  "Visualizzatore foto di Windows" (quello di Windows 7/10) e attiva
  le miniature in Esplora file.

  Non installa nessun programma che resta in esecuzione: usa il
  decoder HEIF ufficiale di Microsoft (WIC) e registra i formati nel
  Visualizzatore foto già presente in Windows.

  Uso:
    Installa :  doppio clic su "Installa.bat"
    Rimuovi  :  doppio clic su "Disinstalla.bat"
#>
param([switch]$Uninstall)

$ErrorActionPreference = 'Stop'

# --- Riavvio come amministratore -------------------------------------------
$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    if ($Uninstall) { $argList += ' -Uninstall' }
    Start-Process powershell.exe -Verb RunAs -ArgumentList $argList
    exit
}

$Host.UI.RawUI.WindowTitle = 'HEIC Viewer per Windows'
$ProgId     = 'PhotoViewer.FileAssoc.Heic'
$Extensions = @('.heic', '.heif', '.hif')
$ViewerDll  = Join-Path $env:ProgramFiles 'Windows Photo Viewer\PhotoViewer.dll'
$Classes    = 'HKLM:\SOFTWARE\Classes'
$CapKey     = 'HKLM:\SOFTWARE\Microsoft\Windows Photo Viewer\Capabilities'
$AssocKey   = "$CapKey\FileAssociations"

function Step($msg)  { Write-Host "`n> $msg" -ForegroundColor Cyan }
function Ok($msg)    { Write-Host "  [OK] $msg" -ForegroundColor Green }
function Warn($msg)  { Write-Host "  [!]  $msg" -ForegroundColor Yellow }

function Set-Default($path, $value, [switch]$Expand) {
    if (-not (Test-Path $path)) { New-Item -Path $path -Force | Out-Null }
    $type = if ($Expand) { 'ExpandString' } else { 'String' }
    New-ItemProperty -Path $path -Name '(default)' -Value $value -PropertyType $type -Force | Out-Null
}

function Set-Value($path, $name, $value, $type = 'String') {
    if (-not (Test-Path $path)) { New-Item -Path $path -Force | Out-Null }
    New-ItemProperty -Path $path -Name $name -Value $value -PropertyType $type -Force | Out-Null
}

function Refresh-Shell {
    Add-Type -Namespace Win32 -Name Shell -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("shell32.dll")]
public static extern void SHChangeNotify(int eventId, uint flags, System.IntPtr a, System.IntPtr b);
'@ -ErrorAction SilentlyContinue
    [Win32.Shell]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)  # SHCNE_ASSOCCHANGED
}

# =============================== DISINSTALLA ================================
if ($Uninstall) {
    Step 'Rimozione associazioni HEIC dal Visualizzatore foto'
    Remove-Item "$Classes\$ProgId" -Recurse -Force -ErrorAction SilentlyContinue
    foreach ($ext in $Extensions) {
        Remove-ItemProperty -Path $AssocKey -Name $ext -ErrorAction SilentlyContinue
        Remove-ItemProperty -Path "$Classes\$ext\OpenWithProgids" -Name $ProgId -ErrorAction SilentlyContinue
    }
    Refresh-Shell
    Ok 'Fatto. I file HEIC tornano ad aprirsi con l''app predefinita.'
    Read-Host "`nPremi INVIO per chiudere"
    exit
}

# ================================ INSTALLA ==================================
Write-Host '==============================================' -ForegroundColor White
Write-Host '   HEIC Viewer - foto iPhone su Windows       ' -ForegroundColor White
Write-Host '==============================================' -ForegroundColor White

# 1) Visualizzatore foto di Windows presente?
Step 'Controllo Visualizzatore foto di Windows'
if (-not (Test-Path $ViewerDll)) {
    Warn "Non trovo $ViewerDll"
    Warn 'Il Visualizzatore foto classico non è presente su questo PC: impossibile continuare.'
    Read-Host "`nPremi INVIO per chiudere"; exit 1
}
Ok 'Trovato.'

# 2) Decoder HEIF + HEVC (servono per aprire le foto e per le miniature)
Step 'Controllo estensioni Microsoft per HEIC'
$heif = Get-AppxPackage -Name 'Microsoft.HEIFImageExtension' -ErrorAction SilentlyContinue
$hevc = Get-AppxPackage -Name 'Microsoft.HEVCVideoExtension*' -ErrorAction SilentlyContinue
$missing = $false

if ($heif) { Ok "Estensioni immagine HEIF: v$($heif.Version)" }
else {
    $missing = $true
    Warn 'Manca "Estensioni immagine HEIF" (gratuita). Apro il Microsoft Store...'
    Start-Process 'ms-windows-store://pdp/?ProductId=9PMMSR1CGPWG'
}

if ($hevc) { Ok "Estensioni video HEVC: v$($hevc[0].Version)" }
else {
    $missing = $true
    Warn 'Manca "Estensioni video HEVC": senza questa le foto HEIC restano nere/illeggibili.'
    Warn 'Apro lo Store sulla versione "dal produttore del dispositivo" (gratuita se disponibile).'
    Warn 'Se lo Store dice "non disponibile", installa quella ufficiale a 0,99 EUR (ID 9NMZLZ57R3T7).'
    Start-Process 'ms-windows-store://pdp/?ProductId=9N4WGH0Z6VHQ'
}

if ($missing) {
    Write-Host ''
    Read-Host '  Installa le estensioni dallo Store, poi premi INVIO per continuare'
}

# 3) Registrazione del formato HEIC nel Visualizzatore foto
Step 'Registrazione di HEIC nel Visualizzatore foto di Windows'
$cmd = '%SystemRoot%\System32\rundll32.exe "%ProgramFiles%\Windows Photo Viewer\PhotoViewer.dll", ImageView_Fullscreen %1'

Set-Default "$Classes\$ProgId" 'Immagine HEIC'
Set-Value   "$Classes\$ProgId" 'FriendlyTypeName' 'Immagine HEIC'
Set-Default "$Classes\$ProgId\DefaultIcon" '%SystemRoot%\System32\imageres.dll,-72' -Expand
Set-Value   "$Classes\$ProgId\shell\open" 'MuiVerb' '@%ProgramFiles%\Windows Photo Viewer\photoviewer.dll,-3043' 'ExpandString'
Set-Default "$Classes\$ProgId\shell\open\command" $cmd -Expand
Set-Value   "$Classes\$ProgId\shell\open\DropTarget" 'Clsid' '{FFE2A43C-56B9-4bf5-9A79-CC6D4285608A}'

# Capabilities del Visualizzatore (lo fa comparire in "Apri con" e in App predefinite)
if (-not (Get-ItemProperty -Path $CapKey -Name 'ApplicationName' -ErrorAction SilentlyContinue)) {
    Set-Value $CapKey 'ApplicationName' '@%ProgramFiles%\Windows Photo Viewer\photoviewer.dll,-3009' 'ExpandString'
    Set-Value $CapKey 'ApplicationDescription' '@%ProgramFiles%\Windows Photo Viewer\photoviewer.dll,-3069' 'ExpandString'
}
Set-Value 'HKLM:\SOFTWARE\RegisteredApplications' 'Windows Photo Viewer' 'SOFTWARE\Microsoft\Windows Photo Viewer\Capabilities'

foreach ($ext in $Extensions) {
    Set-Value $AssocKey $ext $ProgId
    Set-Value "$Classes\$ext\OpenWithProgids" $ProgId ''
    if (-not (Get-ItemProperty -Path "$Classes\$ext" -Name 'PerceivedType' -ErrorAction SilentlyContinue)) {
        Set-Value "$Classes\$ext" 'PerceivedType' 'image'
    }
    Ok "$ext registrato"
}

# 4) Miniature
Step 'Miniature in Esplora file'
Ok 'Le genera il decoder HEIF di Windows (nessun programma in background).'
$ans = Read-Host '  Vuoi svuotare la cache delle miniature per rigenerarle subito? (S/N)'
if ($ans -match '^[sSyY]') {
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    Get-ChildItem "$env:LOCALAPPDATA\Microsoft\Windows\Explorer" -Filter 'thumbcache_*.db' -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
    Start-Process explorer.exe
    Ok 'Cache svuotata, Esplora file riavviato.'
}

Refresh-Shell

# 5) Ultimo passo (Windows non permette di impostare l'app predefinita da script)
Step 'ULTIMO PASSO (una volta sola)'
Write-Host '  1. Tasto destro su una foto .heic  >  Apri con  >  Scegli un''altra app'
Write-Host '  2. Seleziona "Visualizzatore foto di Windows"'
Write-Host '  3. Spunta "Usa sempre questa app" e premi OK'
Write-Host '  (Ripeti per .heif solo se hai anche quei file.)'
Write-Host "`nInstallazione completata." -ForegroundColor Green
Read-Host "`nPremi INVIO per chiudere"
