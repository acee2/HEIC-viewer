// HEIC Viewer - generatore di copertine (miniature) per Esplora file.
// Contiene SOLO i thumbnail provider: Windows li esegue in un processo
// separato (DllHost), quindi non possono rallentare Esplora file.
// La registrazione nel registro la fa l'installer.

#include <windows.h>
#include <cwchar>
#include <new>

#include "module.h"
#include "thumbnail.h"

HMODULE g_hModule = nullptr;
std::atomic<long> g_dllRefs{0};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

template <class T>
class ClassFactory : public IClassFactory {
public:
    ClassFactory() { ++g_dllRefs; }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(++m_ref); }
    IFACEMETHODIMP_(ULONG) Release() override {
        long r = --m_ref;
        if (r == 0) delete this;
        return static_cast<ULONG>(r);
    }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        T* obj = new (std::nothrow) T();
        if (!obj) return E_OUTOFMEMORY;
        HRESULT hr = obj->QueryInterface(riid, ppv);
        obj->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock) ++g_dllRefs; else --g_dllRefs;
        return S_OK;
    }

private:
    ~ClassFactory() { --g_dllRefs; }
    std::atomic<long> m_ref{1};
};

// Mai dentro Esplora file o nei processi di ricerca: decodificare lì lo rallenta.
static bool IsShellProcess() {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* name = wcsrchr(exe, L'\\');
    name = name ? name + 1 : exe;
    const wchar_t* blocked[] = {L"explorer.exe", L"SearchProtocolHost.exe", L"SearchIndexer.exe",
                                L"SearchFilterHost.exe", L"SearchApp.exe", L"SearchHost.exe"};
    for (const wchar_t* b : blocked) {
        if (_wcsicmp(name, b) == 0) return true;
    }
    return false;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (IsShellProcess()) return CLASS_E_CLASSNOTAVAILABLE;

    IClassFactory* factory = nullptr;
    if (rclsid == CLSID_HeicThumbnailProvider) {
        factory = new (std::nothrow) ClassFactory<HeicThumbnailProvider>();
    } else if (rclsid == CLSID_HeicVideoThumbnailProvider) {
        factory = new (std::nothrow) ClassFactory<VideoThumbnailProvider>();
    } else {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() { return g_dllRefs.load() == 0 ? S_OK : S_FALSE; }

// La registrazione la gestisce l'installer (chiavi sotto i tipi di file dell'app).
STDAPI DllRegisterServer() { return S_OK; }
STDAPI DllUnregisterServer() { return S_OK; }
