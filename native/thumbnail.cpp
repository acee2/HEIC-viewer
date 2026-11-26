#include "thumbnail.h"
#include "heic_decode.h"
#include "module.h"
#include "video_decode.h"
#include "log.h"

#include <algorithm>
#include <cstring>

HeicThumbnailProvider::HeicThumbnailProvider(bool video) : m_video(video) { ++g_dllRefs; }

HeicThumbnailProvider::~HeicThumbnailProvider() {
    if (m_stream) m_stream->Release();
    --g_dllRefs;
}

IFACEMETHODIMP HeicThumbnailProvider::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IInitializeWithStream)) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == __uuidof(IThumbnailProvider)) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) HeicThumbnailProvider::AddRef() { return static_cast<ULONG>(++m_ref); }

IFACEMETHODIMP_(ULONG) HeicThumbnailProvider::Release() {
    long r = --m_ref;
    if (r == 0) delete this;
    return static_cast<ULONG>(r);
}

IFACEMETHODIMP HeicThumbnailProvider::Initialize(IStream* stream, DWORD grfMode) {
    m_mode = grfMode;
    if (!stream) return E_POINTER;
    if (m_stream) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    m_stream = stream;
    m_stream->AddRef();
    return S_OK;
}

IFACEMETHODIMP HeicThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    if (!phbmp || !pdwAlpha) return E_POINTER;
    *phbmp = nullptr;
    *pdwAlpha = WTSAT_RGB;
    if (!m_stream || cx == 0) return E_UNEXPECTED;

    DecodedImage img;
    HRESULT hr;
    if (m_video) {
        hr = DecodeVideoFrame(m_stream, img);
        if (FAILED(hr)) { LogError(L"Miniatura video", hr); return hr; }
    } else {
        // Le foto che non sono HEIC/HEIF le fa il generatore di Windows (stesse copertine di sempre).
        BYTE header[12] = {};
        ULONG read = 0;
        LARGE_INTEGER zero = {};
        m_stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        m_stream->Read(header, sizeof(header), &read);
        m_stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        if (!LooksLikeHeic(header, read)) return DelegateToWindows(cx, phbmp, pdwAlpha);

        std::vector<BYTE> data;
        hr = ReadAllFromStream(m_stream, data);
        if (SUCCEEDED(hr)) hr = DecodeHeic(data.data(), data.size(), img, cx);
        if (FAILED(hr)) {
            LogError(L"Miniatura HEIC", hr);
            m_stream->Seek(zero, STREAM_SEEK_SET, nullptr);
            return DelegateToWindows(cx, phbmp, pdwAlpha);
        }
    }

    // Adatta al riquadro cx x cx mantenendo le proporzioni.
    UINT w = img.width, h = img.height;
    const UINT longest = std::max(w, h);
    std::vector<BYTE> scaled;
    const BYTE* pixels = img.bgra.data();
    if (longest > cx) {
        UINT nw = std::max<UINT>(1, static_cast<UINT>(static_cast<unsigned long long>(w) * cx / longest));
        UINT nh = std::max<UINT>(1, static_cast<UINT>(static_cast<unsigned long long>(h) * cx / longest));
        hr = ScaleBGRA(img, nw, nh, scaled);
        if (FAILED(hr)) return hr;
        w = nw;
        h = nh;
        pixels = scaled.data();
    }

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = static_cast<LONG>(w);
    bi.bmiHeader.biHeight = -static_cast<LONG>(h);  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) return E_OUTOFMEMORY;
    std::memcpy(bits, pixels, static_cast<size_t>(w) * h * 4);

    *phbmp = bmp;
    return S_OK;
}

// Usa il generatore di copertine delle foto di Windows (quello registrato per il
// tipo "image"), così JPG, PNG, GIF, WEBP... hanno le stesse copertine di sempre.
HRESULT HeicThumbnailProvider::DelegateToWindows(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    // {C7657C4A-9F68-40fa-A4DF-96BC08EB3551}: Photo Thumbnail Provider di Windows
    CLSID clsid = {0xc7657c4a, 0x9f68, 0x40fa, {0xa4, 0xdf, 0x96, 0xbc, 0x08, 0xeb, 0x35, 0x51}};
    wchar_t buf[64] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CLASSES_ROOT, L"SystemFileAssociations\\image\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}",
                     nullptr, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) {
        CLSID found;
        if (SUCCEEDED(CLSIDFromString(buf, &found)) && found != CLSID_HeicThumbnailProvider) clsid = found;
    }

    IInitializeWithStream* init = nullptr;
    HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IInitializeWithStream),
                                  reinterpret_cast<void**>(&init));
    if (FAILED(hr)) {
        LogError(L"Generatore di Windows non disponibile", hr);
        return hr;
    }
    IThumbnailProvider* provider = nullptr;
    hr = init->Initialize(m_stream, m_mode);
    if (SUCCEEDED(hr)) hr = init->QueryInterface(__uuidof(IThumbnailProvider), reinterpret_cast<void**>(&provider));
    if (SUCCEEDED(hr)) hr = provider->GetThumbnail(cx, phbmp, pdwAlpha);
    if (provider) provider->Release();
    init->Release();
    return hr;
}
