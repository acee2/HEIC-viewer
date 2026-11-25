#pragma once
#include <windows.h>

// Scrive una riga in %LOCALAPPDATA%\HEIC Viewer\errori.log (solo in caso di errore).
void LogError(const wchar_t* where, HRESULT hr, const wchar_t* detail = nullptr);
