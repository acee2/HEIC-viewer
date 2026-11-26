#pragma once
#include <windows.h>
#include <objidl.h>

#include "heic_decode.h"

// Estrae un fotogramma (il primo fotogramma chiave) da un video .MOV/.MP4.
// - HEVC (iPhone): decodificato con libde265, gratis, senza codec Microsoft.
// - Altri codec (es. H.264): decodificati con Media Foundation di Windows.
// La rotazione del video (es. video verticali) è già applicata.
HRESULT DecodeVideoFrame(IStream* stream, DecodedImage& img);

// Ruota l'immagine in senso orario di 90/180/270 gradi.
void RotateBGRA(DecodedImage& img, int degreesClockwise);
