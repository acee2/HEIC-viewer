#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <propsys.h>
#include <thumbcache.h>
#include <atomic>

// Genera le miniature dei file HEIC in Esplora file.
class HeicThumbnailProvider : public IInitializeWithStream, public IThumbnailProvider {
public:
    HeicThumbnailProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* stream, DWORD grfMode) override;

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override;

private:
    ~HeicThumbnailProvider();
    std::atomic<long> m_ref{1};
    IStream* m_stream = nullptr;
};
