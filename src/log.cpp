#include "log.h"

#include <cstdio>
#include <cwchar>
#include <string>

void LogError(const wchar_t* where, HRESULT hr, const wchar_t* detail) {
    wchar_t base[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH)) return;
    std::wstring dir = std::wstring(base) + L"\\HEIC Viewer";
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::wstring file = dir + L"\\errori.log";

    // Evita che il file cresca all'infinito.
    WIN32_FILE_ATTRIBUTE_DATA info = {};
    if (GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &info) && info.nFileSizeLow > 1024 * 1024)
        DeleteFileW(file.c_str());

    HANDLE h = CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[1024];
    int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "%04d-%02d-%02d %02d:%02d:%02d [%s] %ls hr=0x%08lX %ls\r\n",
                        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
#ifdef _WIN64
                        "x64",
#else
                        "x86",
#endif
                        where, static_cast<unsigned long>(hr), detail ? detail : L"");
    if (n > 0) {
        DWORD written = 0;
        WriteFile(h, line, static_cast<DWORD>(n), &written, nullptr);
    }
    CloseHandle(h);
}
