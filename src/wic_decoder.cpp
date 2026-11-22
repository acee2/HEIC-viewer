#include "wic_decoder.h"
#include "module.h"

#include <cstring>
#include <new>

// ============================== Decoder ==============================

HeicWicDecoder::HeicWicDecoder() { ++g_dllRefs; }
HeicWicDecoder::~HeicWicDecoder() { --g_dllRefs; }

IFACEMETHODIMP HeicWicDecoder::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IWICBitmapDecoder)) {
        *ppv = static_cast<IWICBitmapDecoder*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) HeicWicDecoder::AddRef() { return static_cast<ULONG>(++m_ref); }

IFACEMETHODIMP_(ULONG) HeicWicDecoder::Release() {
    long r = --m_ref;
    if (r == 0) delete this;
    return static_cast<ULONG>(r);
}

IFACEMETHODIMP HeicWicDecoder::QueryCapability(IStream* pIStream, DWORD* pdwCapability) {
    if (!pIStream || !pdwCapability) return E_POINTER;
    *pdwCapability = 0;

    // Legge l'intestazione e rimette lo stream dov'era.
    LARGE_INTEGER zero = {};
    ULARGE_INTEGER start = {};
    HRESULT hr = pIStream->Seek(zero, STREAM_SEEK_CUR, &start);
    if (FAILED(hr)) return hr;

    BYTE header[12] = {};
    ULONG read = 0;
    hr = pIStream->Read(header, sizeof(header), &read);

    LARGE_INTEGER back;
    back.QuadPart = static_cast<LONGLONG>(start.QuadPart);
    pIStream->Seek(back, STREAM_SEEK_SET, nullptr);

    if (FAILED(hr)) return hr;
    if (LooksLikeHeic(header, read)) {
        *pdwCapability = WICBitmapDecoderCapabilityCanDecodeAllImages;
    }
    return S_OK;
}

IFACEMETHODIMP HeicWicDecoder::Initialize(IStream* pIStream, WICDecodeOptions) {
    if (!pIStream) return E_POINTER;

    std::vector<BYTE> data;
    HRESULT hr = ReadAllFromStream(pIStream, data);
    if (FAILED(hr)) return hr;

    auto image = std::make_shared<DecodedImage>();
    hr = DecodeHeic(data.data(), data.size(), *image);
    if (FAILED(hr)) return hr;

    std::lock_guard<std::mutex> guard(m_lock);
    if (m_image) return WINCODEC_ERR_WRONGSTATE;
    m_image = std::move(image);
    return S_OK;
}

IFACEMETHODIMP HeicWicDecoder::GetContainerFormat(GUID* pguidContainerFormat) {
    if (!pguidContainerFormat) return E_POINTER;
    *pguidContainerFormat = GUID_ContainerFormatHeicViewer;
    return S_OK;
}

IFACEMETHODIMP HeicWicDecoder::GetDecoderInfo(IWICBitmapDecoderInfo** ppIDecoderInfo) {
    if (!ppIDecoderInfo) return E_POINTER;
    *ppIDecoderInfo = nullptr;

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  __uuidof(IWICImagingFactory), reinterpret_cast<void**>(&factory));
    if (FAILED(hr)) return hr;

    IWICComponentInfo* info = nullptr;
    hr = factory->CreateComponentInfo(CLSID_HeicWicDecoder, &info);
    if (SUCCEEDED(hr)) {
        hr = info->QueryInterface(__uuidof(IWICBitmapDecoderInfo), reinterpret_cast<void**>(ppIDecoderInfo));
        info->Release();
    }
    factory->Release();
    return hr;
}

IFACEMETHODIMP HeicWicDecoder::CopyPalette(IWICPalette*) { return WINCODEC_ERR_PALETTEUNAVAILABLE; }

IFACEMETHODIMP HeicWicDecoder::GetMetadataQueryReader(IWICMetadataQueryReader** pp) {
    if (pp) *pp = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

IFACEMETHODIMP HeicWicDecoder::GetPreview(IWICBitmapSource** pp) {
    if (pp) *pp = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

IFACEMETHODIMP HeicWicDecoder::GetColorContexts(UINT, IWICColorContext**, UINT* pcActualCount) {
    if (!pcActualCount) return E_POINTER;
    *pcActualCount = 0;
    return S_OK;
}

IFACEMETHODIMP HeicWicDecoder::GetThumbnail(IWICBitmapSource** pp) {
    if (pp) *pp = nullptr;
    return WINCODEC_ERR_CODECNOTHUMBNAIL;
}

IFACEMETHODIMP HeicWicDecoder::GetFrameCount(UINT* pCount) {
    if (!pCount) return E_POINTER;
    std::lock_guard<std::mutex> guard(m_lock);
    if (!m_image) return WINCODEC_ERR_NOTINITIALIZED;
    *pCount = 1;
    return S_OK;
}

IFACEMETHODIMP HeicWicDecoder::GetFrame(UINT index, IWICBitmapFrameDecode** ppIBitmapFrame) {
    if (!ppIBitmapFrame) return E_POINTER;
    *ppIBitmapFrame = nullptr;

    std::lock_guard<std::mutex> guard(m_lock);
    if (!m_image) return WINCODEC_ERR_NOTINITIALIZED;
    if (index != 0) return WINCODEC_ERR_FRAMEMISSING;

    HeicWicFrame* frame = new (std::nothrow) HeicWicFrame(m_image);
    if (!frame) return E_OUTOFMEMORY;
    *ppIBitmapFrame = frame;  // il riferimento iniziale passa al chiamante
    return S_OK;
}

// =============================== Frame ===============================

HeicWicFrame::HeicWicFrame(std::shared_ptr<const DecodedImage> image) : m_image(std::move(image)) { ++g_dllRefs; }
HeicWicFrame::~HeicWicFrame() { --g_dllRefs; }

IFACEMETHODIMP HeicWicFrame::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IWICBitmapSource) ||
        riid == __uuidof(IWICBitmapFrameDecode)) {
        *ppv = static_cast<IWICBitmapFrameDecode*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) HeicWicFrame::AddRef() { return static_cast<ULONG>(++m_ref); }

IFACEMETHODIMP_(ULONG) HeicWicFrame::Release() {
    long r = --m_ref;
    if (r == 0) delete this;
    return static_cast<ULONG>(r);
}

IFACEMETHODIMP HeicWicFrame::GetSize(UINT* puiWidth, UINT* puiHeight) {
    if (!puiWidth || !puiHeight) return E_POINTER;
    *puiWidth = m_image->width;
    *puiHeight = m_image->height;
    return S_OK;
}

IFACEMETHODIMP HeicWicFrame::GetPixelFormat(WICPixelFormatGUID* pPixelFormat) {
    if (!pPixelFormat) return E_POINTER;
    *pPixelFormat = GUID_WICPixelFormat32bppBGRA;
    return S_OK;
}

IFACEMETHODIMP HeicWicFrame::GetResolution(double* pDpiX, double* pDpiY) {
    if (!pDpiX || !pDpiY) return E_POINTER;
    *pDpiX = 96.0;
    *pDpiY = 96.0;
    return S_OK;
}

IFACEMETHODIMP HeicWicFrame::CopyPalette(IWICPalette*) { return WINCODEC_ERR_PALETTEUNAVAILABLE; }

IFACEMETHODIMP HeicWicFrame::CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer) {
    const UINT w = m_image->width;
    const UINT h = m_image->height;

    WICRect r = {0, 0, static_cast<INT>(w), static_cast<INT>(h)};
    if (prc) r = *prc;

    if (r.X < 0 || r.Y < 0 || r.Width < 0 || r.Height < 0 ||
        static_cast<UINT>(r.X) + static_cast<UINT>(r.Width) > w ||
        static_cast<UINT>(r.Y) + static_cast<UINT>(r.Height) > h) {
        return E_INVALIDARG;
    }
    if (r.Width == 0 || r.Height == 0) return S_OK;
    if (!pbBuffer) return E_POINTER;

    const UINT rowBytes = static_cast<UINT>(r.Width) * 4;
    if (cbStride < rowBytes) return E_INVALIDARG;
    const unsigned long long needed =
        static_cast<unsigned long long>(r.Height - 1) * cbStride + rowBytes;
    if (cbBufferSize < needed) return WINCODEC_ERR_INSUFFICIENTBUFFER;

    const BYTE* src = m_image->bgra.data() + (static_cast<size_t>(r.Y) * w + r.X) * 4;
    for (INT y = 0; y < r.Height; ++y) {
        std::memcpy(pbBuffer + static_cast<size_t>(y) * cbStride, src + static_cast<size_t>(y) * w * 4, rowBytes);
    }
    return S_OK;
}

IFACEMETHODIMP HeicWicFrame::GetMetadataQueryReader(IWICMetadataQueryReader** pp) {
    if (pp) *pp = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

IFACEMETHODIMP HeicWicFrame::GetColorContexts(UINT, IWICColorContext**, UINT* pcActualCount) {
    if (!pcActualCount) return E_POINTER;
    *pcActualCount = 0;
    return S_OK;
}

IFACEMETHODIMP HeicWicFrame::GetThumbnail(IWICBitmapSource** pp) {
    if (pp) *pp = nullptr;
    return WINCODEC_ERR_CODECNOTHUMBNAIL;
}
