#include "thumbnail.h"
#include "heic_decode.h"
#include "module.h"

#include <algorithm>
#include <cstring>

HeicThumbnailProvider::HeicThumbnailProvider() { ++g_dllRefs; }

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

IFACEMETHODIMP HeicThumbnailProvider::Initialize(IStream* stream, DWORD) {
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

    std::vector<BYTE> data;
    HRESULT hr = ReadAllFromStream(m_stream, data);
    if (FAILED(hr)) return hr;

    DecodedImage img;
    hr = DecodeHeic(data.data(), data.size(), img, cx);
    if (FAILED(hr)) return hr;
    data.clear();
    data.shrink_to_fit();

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
