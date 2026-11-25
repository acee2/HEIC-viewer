# HEIC Viewer per Windows

Apre le foto HEIC/HEIF dell'iPhone nel classico **Visualizzatore foto di Windows** e mostra le **miniature in Esplora file**, anche dei **video .MOV** dell'iPhone (HEVC), che si aprono gratis in **Windows Media Player**.
Gratis: usa i decoder open source [libheif](https://github.com/strukturag/libheif) + [libde265](https://github.com/strukturag/libde265), non il codec HEVC a pagamento di Microsoft.

Non c'è nessun programma in esecuzione: è un'estensione di Windows (una DLL) che il sistema carica solo quando serve.

## Cosa contiene

| File | A cosa serve |
|---|---|
| `src/wic_decoder.cpp` | Decoder WIC: insegna a Windows (e al Visualizzatore foto) a leggere i file HEIC |
| `src/thumbnail.cpp` | Generatore di miniature per Esplora file (usa la miniatura incorporata nella foto quando basta, quindi è veloce) |
| `src/video_decode.cpp` | Miniature dei .MOV: legge il file, prende il primo fotogramma chiave e lo decodifica con libde265 (HEVC) o con Windows (H.264) |
| `src/hevc_mft.cpp` | Decoder video HEVC per Media Foundation (libde265): Windows Media Player riproduce i video HEVC senza il codec a pagamento |
| `src/heic_decode.cpp` | Decodifica con libheif, rotazione già applicata |
| `src/dllmain.cpp` | Registrazione COM/WIC nel registro |
| `installer/HeicViewer.iss` | Installer (Inno Setup) con disinstallazione da "App installate" |
| `.github/workflows/build.yml` | Compilazione automatica su GitHub |

## Come ottenere il Setup

1. Crea un repository su GitHub (anche privato) e carica tutti questi file, cartella `.github` compresa.
2. Vai su **Actions** → aspetta che "Build HEIC Viewer" finisca (la prima volta ~10 minuti: compila libheif).
3. Apri l'esecuzione → in fondo, sezione **Artifacts** → scarica **HEIC-Viewer-Setup**.
4. Estrai lo zip e avvia `HEIC-Viewer-Setup.exe`.

Per una release scaricabile: crea un tag `v1.0.0` e fai push, il Setup finisce in **Releases**.

## Dopo l'installazione

Una volta sola per tipo di file: tasto destro → **Apri con** → **Scegli un'altra app** → spunta **Usa sempre questa app** e scegli:
- foto `.HEIC` → **Visualizzatore foto di Windows**
- video `.MOV` / `.MP4` → **Windows Media Player**

Disinstallazione: Impostazioni → App → HEIC Viewer → Disinstalla.
