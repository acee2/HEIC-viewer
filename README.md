# HEIC Viewer 2

App per Windows che apre **foto e video** di qualsiasi tipo, comprese le foto **HEIC** e i video **MOV/HEVC** dell'iPhone, gratis e senza codec di Microsoft.

- **Foto** (HEIC, HEIF, AVIF, JPG, PNG, GIF, WEBP, BMP, TIFF, ICO, JXL, PSD…): visualizzatore in stile Visualizzatore foto di Windows 10, con zoom, rotazione, presentazione, schermo intero, foto precedente/successiva della cartella, elimina nel Cestino.
- **Video** (MP4, MOV, MKV, AVI, WMV, WEBM, 3GP, MTS, TS, MPG…): lettore integrato basato sul motore di VLC (libVLC), con barra di avanzamento, volume, schermo intero.

È un'app normale: **non installa codec nel sistema e non si aggancia a Esplora file**, quindi non può rallentarlo.

## Installazione

Scarica ed esegui il Setup: https://github.com/acee2/HEIC-viewer/releases/download/latest/HEIC-Viewer-Setup.exe

L'installazione **non** imposta HEIC Viewer come app predefinita. Per usarlo:
tasto destro su una foto o un video → **Apri con** → **Scegli un'altra app** → **HEIC Viewer** (spunta "Usa sempre questa app" se vuoi).
Oppure: Impostazioni → App → App predefinite → *Scegli le app predefinite per tipo di file*.

Se sul PC c'è ancora la vecchia versione 1.x, il Setup la disinstalla da solo.

## Tasti

| Tasto | Azione |
|---|---|
| ← / → | Precedente / successivo nella cartella |
| Rotellina, + / − | Zoom (foto) |
| 0 / doppio clic | Adatta alla finestra ↔ dimensioni reali |
| Ctrl+, / Ctrl+. | Ruota a sinistra / destra |
| Spazio | Play/pausa (video), presentazione (foto) |
| Ctrl+← / Ctrl+→ | Indietro / avanti di 5 secondi (video) |
| F11 / Esc | Schermo intero / esci |
| Canc | Sposta nel Cestino |

## Struttura

| Percorso | Contenuto |
|---|---|
| `src/HeicViewer` | App WPF .NET 8 (Magick.NET per le foto, LibVLCSharp per i video) |
| `installer/HeicViewer.iss` | Installer Inno Setup (registra l'app senza renderla predefinita) |
| `.github/workflows/build.yml` | Compilazione e pubblicazione del Setup |

Errori: `%LOCALAPPDATA%\HEIC Viewer\errori.log`.
