// HEIC Viewer - estensione di Windows per le foto HEIC/HEIF dell'iPhone.
// Nessun processo in background: Windows carica questa DLL solo quando
// deve mostrare una miniatura o aprire una foto.

#include <windows.h>
#include <shlobj.h>
#include <wincodec.h>
#include <mfapi.h>
#include <mftransform.h>

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>
#include <cwchar>
#include <new>
#include <string>

#include "module.h"
#include "thumbnail.h"
#include "wic_decoder.h"
#include "hevc_mft.h"

HMODULE g_hModule = nullptr;
std::atomic<long> g_dllRefs{0};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

// ============================ Class factory ============================

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

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    IClassFactory* factory = nullptr;
    if (rclsid == CLSID_HeicThumbnailProvider) {
        factory = new (std::nothrow) ClassFactory<HeicThumbnailProvider>();
    } else if (rclsid == CLSID_HeicVideoThumbnailProvider) {
        factory = new (std::nothrow) ClassFactory<VideoThumbnailProvider>();
    } else if (rclsid == CLSID_HevcDecoderMft) {
        factory = new (std::nothrow) ClassFactory<HevcDecoderMft>();
    } else if (rclsid == CLSID_HeicWicDecoder) {
        factory = new (std::nothrow) ClassFactory<HeicWicDecoder>();
    } else {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    if (!factory) return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() { return g_dllRefs.load() == 0 ? S_OK : S_FALSE; }

// ============================== Registro ==============================

namespace {

const wchar_t* const kClasses = L"Software\\Classes\\";
const wchar_t* const kThumbHandlerKey = L"\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}";
const wchar_t* const kWicDecodersCategory = L"{7ED96837-96F0-4812-B211-F13C24117ED3}";
const wchar_t* const kPixelFormatBGRA = L"{6fddc324-4e03-4bfe-b185-3d77768dc90f}";
const wchar_t* const kPhotoViewerProgId = L"PhotoViewer.FileAssoc.Heic";
const wchar_t* const kExtensions[] = {L".heic", L".heif", L".hif"};
const wchar_t* const kVideoExtensions[] = {L".mov", L".mp4", L".m4v"};
const wchar_t* const kBackupKey = L"Software\\HEIC Viewer\\PreviousVideoThumbnailHandlers";
const char* const kBrands[] = {"heic", "heix", "hevc", "hevx", "heim", "heis", "mif1", "msf1"};

std::wstring GuidStr(const GUID& g) {
    wchar_t buf[64] = {};
    StringFromGUID2(g, buf, 64);
    return buf;
}

HRESULT CreateKey(const std::wstring& sub) {
    HKEY key = nullptr;
    LSTATUS s = RegCreateKeyExW(HKEY_LOCAL_MACHINE, sub.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
    if (s == ERROR_SUCCESS) RegCloseKey(key);
    return HRESULT_FROM_WIN32(s);
}

HRESULT SetValue(const std::wstring& sub, const wchar_t* name, DWORD type, const void* data, DWORD size) {
    HKEY key = nullptr;
    LSTATUS s = RegCreateKeyExW(HKEY_LOCAL_MACHINE, sub.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
    if (s != ERROR_SUCCESS) return HRESULT_FROM_WIN32(s);
    s = RegSetValueExW(key, name, 0, type, static_cast<const BYTE*>(data), size);
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(s);
}

HRESULT SetString(const std::wstring& sub, const wchar_t* name, const std::wstring& value) {
    return SetValue(sub, name, REG_SZ, value.c_str(), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

HRESULT SetDword(const std::wstring& sub, const wchar_t* name, DWORD value) {
    return SetValue(sub, name, REG_DWORD, &value, sizeof(value));
}

HRESULT SetBinary(const std::wstring& sub, const wchar_t* name, const BYTE* data, DWORD size) {
    return SetValue(sub, name, REG_BINARY, data, size);
}

bool GetString(const std::wstring& sub, const wchar_t* name, std::wstring& out) {
    wchar_t buf[256] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, sub.c_str(), name, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return false;
    out = buf;
    return true;
}

void DeleteTree(const std::wstring& sub) { RegDeleteTreeW(HKEY_LOCAL_MACHINE, sub.c_str()); }

void DeleteValue(const std::wstring& sub, const wchar_t* name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, sub.c_str(), 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegDeleteValueW(key, name);
        RegCloseKey(key);
    }
}

#define CHECK(expr)                     \
    do {                                \
        HRESULT hr_ = (expr);           \
        if (FAILED(hr_)) return hr_;    \
    } while (0)

HRESULT RegisterThumbnailProvider(const std::wstring& dllPath) {
    const std::wstring clsid = GuidStr(CLSID_HeicThumbnailProvider);
    const std::wstring base = std::wstring(kClasses) + L"CLSID\\" + clsid;

    CHECK(SetString(base, nullptr, L"HEIC Viewer - Miniature"));
    CHECK(SetString(base + L"\\InprocServer32", nullptr, dllPath));
    CHECK(SetString(base + L"\\InprocServer32", L"ThreadingModel", L"Apartment"));

    for (const wchar_t* ext : kExtensions) {
        const std::wstring e = std::wstring(kClasses) + ext;
        CHECK(SetString(e + kThumbHandlerKey, nullptr, clsid));
        CHECK(SetString(std::wstring(kClasses) + L"SystemFileAssociations\\" + ext + kThumbHandlerKey, nullptr, clsid));
        CHECK(SetString(e, L"PerceivedType", L"image"));
        CHECK(SetString(e, L"Content Type", wcscmp(ext, L".heic") == 0 ? L"image/heic" : L"image/heif"));
        CHECK(SetString(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap", ext, L"picture"));
    }
    CHECK(SetString(std::wstring(kClasses) + kPhotoViewerProgId + kThumbHandlerKey, nullptr, clsid));
    CHECK(SetString(L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", clsid.c_str(),
                    L"HEIC Viewer - Miniature"));
    return S_OK;
}

// Chiavi del registro dove va messo il generatore di miniature video:
// estensione, SystemFileAssociations e i tipi di file (ProgID) a cui l'estensione
// è collegata (Windows Media Player, l'app scelta con "Apri con"...). Un gestore
// registrato sul ProgID ha la precedenza su quello dell'estensione.
std::vector<std::wstring> VideoThumbnailKeys() {
    std::vector<std::wstring> keys;
    std::vector<std::wstring> progIds = {L"WMP11.AssocFile.MOV", L"WMP11.AssocFile.MP4", L"WMP11.AssocFile.M4V"};

    for (const wchar_t* ext : kVideoExtensions) {
        keys.push_back(std::wstring(kClasses) + ext + kThumbHandlerKey);
        keys.push_back(std::wstring(kClasses) + L"SystemFileAssociations\\" + ext + kThumbHandlerKey);

        // ProgID scelto dall'utente
        wchar_t buf[256] = {};
        DWORD size = sizeof(buf);
        const std::wstring userChoice =
            std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\") + ext + L"\\UserChoice";
        if (RegGetValueW(HKEY_CURRENT_USER, userChoice.c_str(), L"ProgId", RRF_RT_REG_SZ, nullptr, buf, &size) ==
                ERROR_SUCCESS && buf[0]) {
            progIds.push_back(buf);
        }
        // ProgID predefinito dell'estensione
        size = sizeof(buf);
        buf[0] = 0;
        if (RegGetValueW(HKEY_CLASSES_ROOT, ext, nullptr, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS &&
            buf[0]) {
            progIds.push_back(buf);
        }
    }

    for (const auto& id : progIds) {
        HKEY k = nullptr;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, id.c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        RegCloseKey(k);
        const std::wstring key = std::wstring(kClasses) + id + kThumbHandlerKey;
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
    }
    return keys;
}

// Registra la miniatura video, salvando i gestori precedenti per
// poterli rimettere alla disinstallazione.
HRESULT RegisterVideoThumbnailProvider(const std::wstring& dllPath) {
    const std::wstring clsid = GuidStr(CLSID_HeicVideoThumbnailProvider);
    const std::wstring base = std::wstring(kClasses) + L"CLSID\\" + clsid;

    CHECK(SetString(base, nullptr, L"HEIC Viewer - Miniature video"));
    CHECK(SetString(base + L"\\InprocServer32", nullptr, dllPath));
    CHECK(SetString(base + L"\\InprocServer32", L"ThreadingModel", L"Apartment"));

    // Aggiornamento dalla 1.1/1.2: recupera i gestori originali salvati allora.
    {
        const wchar_t* oldBackup = L"Software\\HEIC Viewer\\PreviousThumbnailHandlers";
        const std::pair<const wchar_t*, std::wstring> legacy[] = {
            {L"ext.mov", std::wstring(kClasses) + L".mov" + kThumbHandlerKey},
            {L"sfa.mov", std::wstring(kClasses) + L"SystemFileAssociations\\.mov" + kThumbHandlerKey},
        };
        for (const auto& l : legacy) {
            std::wstring old, tmp;
            if (GetString(oldBackup, l.first, old) && !GetString(kBackupKey, l.second.c_str(), tmp))
                SetString(kBackupKey, l.second.c_str(), old);
        }
        DeleteTree(oldBackup);
    }

    for (const auto& key : VideoThumbnailKeys()) {
        std::wstring previous, alreadySaved;
        if (!GetString(kBackupKey, key.c_str(), alreadySaved)) {
            if (!GetString(key, nullptr, previous) || _wcsicmp(previous.c_str(), clsid.c_str()) == 0) previous.clear();
            CHECK(SetString(kBackupKey, key.c_str(), previous));  // "" = non c'era niente
        }
        CHECK(SetString(key, nullptr, clsid));
    }
    for (const wchar_t* ext : kVideoExtensions) {
        CHECK(SetString(std::wstring(kClasses) + ext, L"PerceivedType", L"video"));
        CHECK(SetString(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap", ext, L"video"));
    }
    CHECK(SetString(L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", clsid.c_str(),
                    L"HEIC Viewer - Miniature video"));
    return S_OK;
}

void UnregisterVideoThumbnailProvider() {
    const std::wstring clsid = GuidStr(CLSID_HeicVideoThumbnailProvider);

    HKEY backup = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kBackupKey, 0, KEY_READ, &backup) == ERROR_SUCCESS) {
        std::vector<std::pair<std::wstring, std::wstring>> entries;
        for (DWORD i = 0;; ++i) {
            wchar_t name[1024];
            DWORD nameLen = 1024;
            wchar_t data[256] = {};
            DWORD dataSize = sizeof(data);
            DWORD type = 0;
            if (RegEnumValueW(backup, i, name, &nameLen, nullptr, &type, reinterpret_cast<BYTE*>(data), &dataSize) !=
                ERROR_SUCCESS)
                break;
            if (type == REG_SZ) entries.emplace_back(name, data);
        }
        RegCloseKey(backup);

        for (const auto& e : entries) {
            std::wstring current;
            if (!GetString(e.first, nullptr, current) || _wcsicmp(current.c_str(), clsid.c_str()) != 0) continue;
            if (e.second.empty()) DeleteTree(e.first);
            else SetString(e.first, nullptr, e.second);
        }
    }
    DeleteTree(std::wstring(kClasses) + L"CLSID\\" + clsid);
    DeleteValue(L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", clsid.c_str());
    DeleteTree(L"Software\\HEIC Viewer");
}

// Decoder video HEVC per Media Foundation (Windows Media Player ecc.).
HRESULT RegisterHevcDecoderMft(const std::wstring& dllPath) {
    const std::wstring clsid = GuidStr(CLSID_HevcDecoderMft);
    const std::wstring base = std::wstring(kClasses) + L"CLSID\\" + clsid;
    CHECK(SetString(base, nullptr, L"HEIC Viewer - Decoder video HEVC"));
    CHECK(SetString(base + L"\\InprocServer32", nullptr, dllPath));
    CHECK(SetString(base + L"\\InprocServer32", L"ThreadingModel", L"Both"));

    MFT_REGISTER_TYPE_INFO inputs[] = {
        {MFMediaType_Video, MFVideoFormat_HEVC},
        {MFMediaType_Video, MFVideoFormat_HEVC_ES},
    };
    MFT_REGISTER_TYPE_INFO outputs[] = {
        {MFMediaType_Video, MFVideoFormat_NV12},
    };
    wchar_t name[] = L"HEIC Viewer HEVC Decoder (libde265)";
    return MFTRegister(CLSID_HevcDecoderMft, MFT_CATEGORY_VIDEO_DECODER, name, 0, 2, inputs, 1, outputs, nullptr);
}

void UnregisterHevcDecoderMft() {
    MFTUnregister(CLSID_HevcDecoderMft);
    DeleteTree(std::wstring(kClasses) + L"CLSID\\" + GuidStr(CLSID_HevcDecoderMft));
}

HRESULT RegisterWicDecoder(const std::wstring& dllPath) {
    const std::wstring clsid = GuidStr(CLSID_HeicWicDecoder);
    const std::wstring base = std::wstring(kClasses) + L"CLSID\\" + clsid;
    const wchar_t* name = L"HEIC Viewer - Decoder HEIF/HEIC";

    CHECK(SetString(base, nullptr, name));
    CHECK(SetString(base, L"FriendlyName", name));
    CHECK(SetString(base, L"Author", L"HEIC Viewer"));
    CHECK(SetString(base, L"Description", L"Decoder HEIC/HEIF basato su libheif e libde265"));
    CHECK(SetString(base, L"Vendor", GuidStr(GUID_VendorHeicViewer)));
    CHECK(SetString(base, L"ContainerFormat", GuidStr(GUID_ContainerFormatHeicViewer)));
    CHECK(SetString(base, L"Version", L"1.3.0.0"));
    CHECK(SetString(base, L"Date", L"2026-11-22"));
    CHECK(SetString(base, L"SpecVersion", L"1.0.0.0"));
    CHECK(SetString(base, L"ColorManagementVersion", L"1.0.0.0"));
    CHECK(SetString(base, L"MimeTypes", L"image/heic,image/heif"));
    CHECK(SetString(base, L"FileExtensions", L".heic,.heif,.hif"));
    CHECK(SetDword(base, L"SupportAnimation", 0));
    CHECK(SetDword(base, L"SupportChromakey", 0));
    CHECK(SetDword(base, L"SupportLossless", 0));
    CHECK(SetDword(base, L"SupportMultiframe", 0));

    CHECK(SetString(base + L"\\InprocServer32", nullptr, dllPath));
    CHECK(SetString(base + L"\\InprocServer32", L"ThreadingModel", L"Both"));
    CHECK(CreateKey(base + L"\\Formats\\" + kPixelFormatBGRA));

    // Riconoscimento del file: "ftyp" + marca a partire dal byte 4.
    const BYTE mask[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    int index = 0;
    for (const char* brand : kBrands) {
        BYTE pattern[8] = {'f', 't', 'y', 'p', 0, 0, 0, 0};
        memcpy(pattern + 4, brand, 4);
        const std::wstring p = base + L"\\Patterns\\" + std::to_wstring(index++);
        CHECK(SetDword(p, L"Position", 4));
        CHECK(SetDword(p, L"Length", 8));
        CHECK(SetBinary(p, L"Pattern", pattern, 8));
        CHECK(SetBinary(p, L"Mask", mask, 8));
    }

    const std::wstring instance =
        std::wstring(kClasses) + L"CLSID\\" + kWicDecodersCategory + L"\\Instance\\" + clsid;
    CHECK(SetString(instance, L"CLSID", clsid));
    CHECK(SetString(instance, L"FriendlyName", name));
    return S_OK;
}

}  // namespace

STDAPI DllRegisterServer() {
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(g_hModule, path, MAX_PATH)) return HRESULT_FROM_WIN32(GetLastError());

    HRESULT hr = RegisterWicDecoder(path);
    if (SUCCEEDED(hr)) hr = RegisterHevcDecoderMft(path);
#ifdef _WIN64
    // Esplora file è a 64 bit: le miniature le registra solo la DLL a 64 bit.
    // La DLL a 32 bit serve ai programmi a 32 bit (es. Windows Media Player).
    if (SUCCEEDED(hr)) hr = RegisterThumbnailProvider(path);
    if (SUCCEEDED(hr)) hr = RegisterVideoThumbnailProvider(path);
#endif
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return hr;
}

STDAPI DllUnregisterServer() {
    const std::wstring thumb = GuidStr(CLSID_HeicThumbnailProvider);
    const std::wstring dec = GuidStr(CLSID_HeicWicDecoder);

#ifdef _WIN64
    DeleteTree(std::wstring(kClasses) + L"CLSID\\" + thumb);
    DeleteTree(std::wstring(kClasses) + L"CLSID\\" + dec);
    DeleteTree(std::wstring(kClasses) + L"CLSID\\" + kWicDecodersCategory + L"\\Instance\\" + dec);
    for (const wchar_t* ext : kExtensions) {
        DeleteTree(std::wstring(kClasses) + ext + kThumbHandlerKey);
        DeleteTree(std::wstring(kClasses) + L"SystemFileAssociations\\" + ext + kThumbHandlerKey);
    }
    DeleteTree(std::wstring(kClasses) + kPhotoViewerProgId + kThumbHandlerKey);
    DeleteValue(L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", thumb.c_str());
    UnregisterVideoThumbnailProvider();
#else
    DeleteTree(std::wstring(kClasses) + L"CLSID\\" + dec);
    DeleteTree(std::wstring(kClasses) + L"CLSID\\" + kWicDecodersCategory + L"\\Instance\\" + dec);
#endif
    UnregisterHevcDecoderMft();

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}
