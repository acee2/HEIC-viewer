#pragma once
#include <windows.h>

// Scrive una riga in %LOCALAPPDATA%\HEIC Viewer\errori.log (solo in caso di errore).
void LogError(const wchar_t* where, HRESULT hr, const wchar_t* detail = nullptr);

// Riga informativa (per capire cosa viene chiamato). Limitata per processo.
void LogInfo(const wchar_t* where, const wchar_t* detail = nullptr);
