#pragma once
#include <windows.h>
#include <atomic>

// Stato globale della DLL delle copertine.
extern HMODULE g_hModule;
extern std::atomic<long> g_dllRefs;

// Generatore di copertine per le foto  {6E1B3C53-8F0A-4D7B-9C1E-2A5D4F7B8C90}
inline constexpr GUID CLSID_HeicThumbnailProvider =
    {0x6e1b3c53, 0x8f0a, 0x4d7b, {0x9c, 0x1e, 0x2a, 0x5d, 0x4f, 0x7b, 0x8c, 0x90}};

// Generatore di copertine per i video  {6E1B3C54-8F0A-4D7B-9C1E-2A5D4F7B8C90}
inline constexpr GUID CLSID_HeicVideoThumbnailProvider =
    {0x6e1b3c54, 0x8f0a, 0x4d7b, {0x9c, 0x1e, 0x2a, 0x5d, 0x4f, 0x7b, 0x8c, 0x90}};
