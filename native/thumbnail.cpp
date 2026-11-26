#include "thumbnail.h"
#include "heic_decode.h"
#include "module.h"
#include "video_decode.h"
#include "log.h"

#include <wincodec.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

enum class Kind { Heic, Video, Image };

bool Has(const BYTE* h, ULONG n, ULONG at, const char* sig) {
    const size_t len = std::strlen(sig);
    return n >= at + len && std::memcmp(h + at, sig, len) == 0;
}

// Riconosce foto HEIC, video (con il tipo MIME per Media Foundation) e altre foto.
Kind Classify(const BYTE* h, ULONG n, const wchar_t*& videoType) {
    videoType = nullptr;
    if (LooksLikeHeic(h, n)) return Kind::Heic;
    if (Has(h, n, 4, "ftyp")) {
        // AVIF è una foto, il resto (mp4, mov/qt, m4v, 3gp...) è video.
        if (Has(h, n, 8, "avif") || Has(h, n, 8, "avis")) return Kind::Image;
        videoType = L"video/mp4";  // il gestore MP4 di Windows legge anche i .mov
        return Kind::Video;
    }
    // QuickTime vecchi senza "ftyp" (iniziano con moov/mdat/wide/free).
    if (Has(h, n, 4, "moov") || Has(h, n, 4, "mdat") || Has(h, n, 4, "wide") || Has(h, n, 4, "free")) {
        videoType = L"video/mp4";
        return Kind::Video;
    }
    if (n >= 4 && h[0] == 0x1A && h[1] == 0x45 && h[2] == 0xDF && h[3] == 0xA3) {  // Matroska / WebM
        videoType = L"video/x-matroska";
        return Kind::Video;
    }
    if (Has(h, n, 0, "RIFF") && Has(h, n, 8, "AVI ")) { videoType = L"video/avi"; return Kind::Video; }
    if (n >= 4 && h[0] == 0x30 && h[1] == 0x26 && h[2] == 0xB2 && h[3] == 0x75) {    // ASF / WMV
        videoType = L"video/x-ms-asf";
        return Kind::Video;
    }
    if (n >= 4 && h[0] == 0 && h[1] == 0 && h[2] == 1 && (h[3] == 0xBA || h[3] == 0xB3)) {  // MPEG-PS
        videoType = L"video/mpeg";
        return Kind::Video;
    }
    if (n >= 1 && h[0] == 0x47 && !Has(h, n, 0, "GIF8")) {  // MPEG-TS
        videoType = L"video/mp2t";
        return Kind::Video;
    }
    return Kind::Image;
}

// Decodifica una foto con WIC (decoder di Windows), già ridotta e girata secondo l'EXIF.
HRESULT DecodeWithWic(IStream* stream, UINT cx, DecodedImage& img) {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    IWICMetadataQueryReader* meta = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  __uuidof(IWICImagingFactory), reinterpret_cast<void**>(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);

    UINT w = 0, h = 0;
    if (SUCCEEDED(hr)) hr = frame->GetSize(&w, &h);
    if (SUCCEEDED(hr) && (w == 0 || h == 0)) hr = E_FAIL;

    // Orientamento EXIF (foto verticali dei telefoni).
    int rotation = 0;
    if (SUCCEEDED(hr) && SUCCEEDED(frame->GetMetadataQueryReader(&meta))) {
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(meta->GetMetadataByName(L"System.Photo.Orientation", &v)) && v.vt == VT_UI2) {
            switch (v.uiVal) {
                case 3: case 4: rotation = 180; break;
                case 5: case 6: rotation = 90; break;
                case 7: case 8: rotation = 270; break;
                default: break;
            }
        }
        PropVariantClear(&v);
    }

    UINT nw = w, nh = h;
    const UINT longest = std::max(w, h);
    if (SUCCEEDED(hr) && longest > cx) {
        nw = std::max<UINT>(1, static_cast<UINT>(static_cast<unsigned long long>(w) * cx / longest));
        nh = std::max<UINT>(1, static_cast<UINT>(static_cast<unsigned long long>(h) * cx / longest));
    }
    if (SUCCEEDED(hr)) hr = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr)) hr = scaler->Initialize(frame, nw, nh, WICBitmapInterpolationModeFant);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr))
        hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(hr)) {
        img.width = nw;
        img.height = nh;
        img.bgra.resize(static_cast<size_t>(nw) * nh * 4);
        hr = converter->CopyPixels(nullptr, nw * 4, static_cast<UINT>(img.bgra.size()), img.bgra.data());
    }
    if (SUCCEEDED(hr)) {
        // Copertine senza trasparenza: il nero dei pixel trasparenti diventerebbe un riquadro nero.
        for (size_t i = 0; i < img.bgra.size(); i += 4) {
            const unsigned a = img.bgra[i + 3];
            if (a == 255) continue;
            for (int c = 0; c < 3; ++c) img.bgra[i + c] = static_cast<BYTE>((img.bgra[i + c] * a + 255 * (255 - a)) / 255);
            img.bgra[i + 3] = 255;
        }
        RotateBGRA(img, rotation);
    }

    if (meta) meta->Release();
    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    return hr;
}

}  // namespace

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

    // Riconosce il contenuto dai primi byte: lo stesso generatore funziona per
    // foto e video, qualunque sia il tipo di file a cui Windows l'ha collegato.
    BYTE header[64] = {};
    ULONG read = 0;
    LARGE_INTEGER zero = {};
    m_stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    m_stream->Read(header, sizeof(header), &read);
    m_stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    const wchar_t* videoType = nullptr;
    const Kind kind = Classify(header, read, videoType);

    DecodedImage img;
    HRESULT hr = E_FAIL;
    if (kind == Kind::Video) {
        hr = DecodeVideoFrame(m_stream, img, videoType);
        if (FAILED(hr)) { LogError(L"Copertina video", hr, videoType); return hr; }
    } else if (kind == Kind::Heic) {
        std::vector<BYTE> data;
        hr = ReadAllFromStream(m_stream, data);
        if (SUCCEEDED(hr)) hr = DecodeHeic(data.data(), data.size(), img, cx);
        if (FAILED(hr)) LogError(L"Copertina HEIC", hr);
    } else {
        hr = DecodeWithWic(m_stream, cx, img);
        if (FAILED(hr)) LogError(L"Copertina foto (WIC)", hr);
    }

    if (FAILED(hr) && kind != Kind::Video) {
        // Ultima risorsa: il generatore di copertine di Windows.
        m_stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        hr = DelegateToWindows(cx, phbmp, pdwAlpha);
        if (FAILED(hr)) LogError(L"Copertina foto (generatore di Windows)", hr);
        return hr;
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
