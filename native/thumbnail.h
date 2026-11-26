#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <propsys.h>
#include <thumbcache.h>
#include <atomic>

// Genera le miniature in Esplora file: foto HEIC oppure video .MOV.
class HeicThumbnailProvider : public IInitializeWithStream, public IThumbnailProvider {
public:
    explicit HeicThumbnailProvider(bool video = false);

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* stream, DWORD grfMode) override;

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override;

protected:
    virtual ~HeicThumbnailProvider();

private:
    HRESULT DelegateToWindows(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha);

    std::atomic<long> m_ref{1};
    IStream* m_stream = nullptr;
    DWORD m_mode = 0;
    bool m_video = false;
};

class VideoThumbnailProvider : public HeicThumbnailProvider {
public:
    VideoThumbnailProvider() : HeicThumbnailProvider(true) {}
};
