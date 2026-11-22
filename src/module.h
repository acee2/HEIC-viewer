#pragma once
#include <windows.h>
#include <atomic>

// Stato globale della DLL (conteggio oggetti COM vivi e handle del modulo).
extern HMODULE g_hModule;
extern std::atomic<long> g_dllRefs;

// {B3B9A4E1-6C2D-4F5A-9E1B-0D7C2A1F4E10}
inline constexpr GUID CLSID_HeicThumbnailProvider =
    {0xb3b9a4e1, 0x6c2d, 0x4f5a, {0x9e, 0x1b, 0x0d, 0x7c, 0x2a, 0x1f, 0x4e, 0x10}};

// {B3B9A4E2-6C2D-4F5A-9E1B-0D7C2A1F4E10}
inline constexpr GUID CLSID_HeicWicDecoder =
    {0xb3b9a4e2, 0x6c2d, 0x4f5a, {0x9e, 0x1b, 0x0d, 0x7c, 0x2a, 0x1f, 0x4e, 0x10}};

// {B3B9A4E3-6C2D-4F5A-9E1B-0D7C2A1F4E10}
inline constexpr GUID GUID_ContainerFormatHeicViewer =
    {0xb3b9a4e3, 0x6c2d, 0x4f5a, {0x9e, 0x1b, 0x0d, 0x7c, 0x2a, 0x1f, 0x4e, 0x10}};

// {B3B9A4E4-6C2D-4F5A-9E1B-0D7C2A1F4E10}
inline constexpr GUID GUID_VendorHeicViewer =
    {0xb3b9a4e4, 0x6c2d, 0x4f5a, {0x9e, 0x1b, 0x0d, 0x7c, 0x2a, 0x1f, 0x4e, 0x10}};
