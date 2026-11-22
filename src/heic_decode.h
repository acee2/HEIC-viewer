#pragma once
#include <windows.h>
#include <objidl.h>
#include <vector>

// Immagine decodificata in memoria, pixel BGRA 8 bit (non premoltiplicati), righe compatte.
struct DecodedImage {
    UINT width = 0;
    UINT height = 0;
    std::vector<BYTE> bgra;
};

// Legge tutto lo stream dalla posizione corrente fino alla fine.
HRESULT ReadAllFromStream(IStream* stream, std::vector<BYTE>& out);

// True se i primi byte sono un contenitore HEIF/HEIC ("....ftypheic" ecc.).
bool LooksLikeHeic(const BYTE* data, size_t size);

// Decodifica l'immagine principale (rotazione/specchiatura già applicate).
// Se preferThumbAtLeast > 0 usa la miniatura incorporata quando il suo lato
// lungo è almeno quella dimensione (molto più veloce per Esplora file).
HRESULT DecodeHeic(const BYTE* data, size_t size, DecodedImage& img, UINT preferThumbAtLeast = 0);

// Ridimensiona con lo scaler di WIC (Fant, alta qualità in riduzione).
HRESULT ScaleBGRA(const DecodedImage& src, UINT newWidth, UINT newHeight, std::vector<BYTE>& out);
