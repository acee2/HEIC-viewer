#pragma once
#include <windows.h>
#include <objidl.h>

#include "heic_decode.h"

// Estrae un fotogramma (il primo fotogramma chiave) da un video .MOV/.MP4.
// - HEVC (iPhone): decodificato con libde265, gratis, senza codec Microsoft.
// - Altri codec (es. H.264): decodificati con Media Foundation di Windows.
// La rotazione del video (es. video verticali) è già applicata.
// contentType: tipo MIME suggerito a Media Foundation (es. L"video/mp4"), può essere nullptr.
HRESULT DecodeVideoFrame(IStream* stream, DecodedImage& img, const wchar_t* contentType = nullptr);

// Ruota l'immagine in senso orario di 90/180/270 gradi.
void RotateBGRA(DecodedImage& img, int degreesClockwise);
