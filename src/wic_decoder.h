#pragma once
#include <windows.h>
#include <wincodec.h>
#include <atomic>
#include <memory>
#include <mutex>

#include "heic_decode.h"

// Decoder WIC: è quello che permette al Visualizzatore foto di Windows
// (e a qualsiasi programma che usa WIC) di aprire i file HEIC.
class HeicWicDecoder : public IWICBitmapDecoder {
public:
    HeicWicDecoder();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP QueryCapability(IStream* pIStream, DWORD* pdwCapability) override;
    IFACEMETHODIMP Initialize(IStream* pIStream, WICDecodeOptions cacheOptions) override;
    IFACEMETHODIMP GetContainerFormat(GUID* pguidContainerFormat) override;
    IFACEMETHODIMP GetDecoderInfo(IWICBitmapDecoderInfo** ppIDecoderInfo) override;
    IFACEMETHODIMP CopyPalette(IWICPalette* pIPalette) override;
    IFACEMETHODIMP GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader) override;
    IFACEMETHODIMP GetPreview(IWICBitmapSource** ppIBitmapSource) override;
    IFACEMETHODIMP GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount) override;
    IFACEMETHODIMP GetThumbnail(IWICBitmapSource** ppIThumbnail) override;
    IFACEMETHODIMP GetFrameCount(UINT* pCount) override;
    IFACEMETHODIMP GetFrame(UINT index, IWICBitmapFrameDecode** ppIBitmapFrame) override;

private:
    ~HeicWicDecoder();
    std::atomic<long> m_ref{1};
    std::mutex m_lock;
    std::shared_ptr<const DecodedImage> m_image;
};

class HeicWicFrame : public IWICBitmapFrameDecode {
public:
    explicit HeicWicFrame(std::shared_ptr<const DecodedImage> image);

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IWICBitmapSource
    IFACEMETHODIMP GetSize(UINT* puiWidth, UINT* puiHeight) override;
    IFACEMETHODIMP GetPixelFormat(WICPixelFormatGUID* pPixelFormat) override;
    IFACEMETHODIMP GetResolution(double* pDpiX, double* pDpiY) override;
    IFACEMETHODIMP CopyPalette(IWICPalette* pIPalette) override;
    IFACEMETHODIMP CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer) override;

    // IWICBitmapFrameDecode
    IFACEMETHODIMP GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader) override;
    IFACEMETHODIMP GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount) override;
    IFACEMETHODIMP GetThumbnail(IWICBitmapSource** ppIThumbnail) override;

private:
    ~HeicWicFrame();
    std::atomic<long> m_ref{1};
    std::shared_ptr<const DecodedImage> m_image;
};
