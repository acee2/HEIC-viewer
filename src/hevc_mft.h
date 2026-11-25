#pragma once
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

// Decoder video HEVC (H.265) per Media Foundation, basato su libde265.
// Registrato in Windows, permette a Windows Media Player (e alle altre app
// che usano Media Foundation) di riprodurre i video .MOV/.MP4 dell'iPhone
// senza il codec HEVC a pagamento di Microsoft.
class HevcDecoderMft : public IMFTransform {
public:
    HevcDecoderMft();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IMFTransform
    IFACEMETHODIMP GetStreamLimits(DWORD* pdwInputMinimum, DWORD* pdwInputMaximum, DWORD* pdwOutputMinimum,
                                   DWORD* pdwOutputMaximum) override;
    IFACEMETHODIMP GetStreamCount(DWORD* pcInputStreams, DWORD* pcOutputStreams) override;
    IFACEMETHODIMP GetStreamIDs(DWORD dwInputIDArraySize, DWORD* pdwInputIDs, DWORD dwOutputIDArraySize,
                                DWORD* pdwOutputIDs) override;
    IFACEMETHODIMP GetInputStreamInfo(DWORD dwInputStreamID, MFT_INPUT_STREAM_INFO* pStreamInfo) override;
    IFACEMETHODIMP GetOutputStreamInfo(DWORD dwOutputStreamID, MFT_OUTPUT_STREAM_INFO* pStreamInfo) override;
    IFACEMETHODIMP GetAttributes(IMFAttributes** pAttributes) override;
    IFACEMETHODIMP GetInputStreamAttributes(DWORD dwInputStreamID, IMFAttributes** pAttributes) override;
    IFACEMETHODIMP GetOutputStreamAttributes(DWORD dwOutputStreamID, IMFAttributes** pAttributes) override;
    IFACEMETHODIMP DeleteInputStream(DWORD dwStreamID) override;
    IFACEMETHODIMP AddInputStreams(DWORD cStreams, DWORD* adwStreamIDs) override;
    IFACEMETHODIMP GetInputAvailableType(DWORD dwInputStreamID, DWORD dwTypeIndex, IMFMediaType** ppType) override;
    IFACEMETHODIMP GetOutputAvailableType(DWORD dwOutputStreamID, DWORD dwTypeIndex, IMFMediaType** ppType) override;
    IFACEMETHODIMP SetInputType(DWORD dwInputStreamID, IMFMediaType* pType, DWORD dwFlags) override;
    IFACEMETHODIMP SetOutputType(DWORD dwOutputStreamID, IMFMediaType* pType, DWORD dwFlags) override;
    IFACEMETHODIMP GetInputCurrentType(DWORD dwInputStreamID, IMFMediaType** ppType) override;
    IFACEMETHODIMP GetOutputCurrentType(DWORD dwOutputStreamID, IMFMediaType** ppType) override;
    IFACEMETHODIMP GetInputStatus(DWORD dwInputStreamID, DWORD* pdwFlags) override;
    IFACEMETHODIMP GetOutputStatus(DWORD* pdwFlags) override;
    IFACEMETHODIMP SetOutputBounds(LONGLONG hnsLowerBound, LONGLONG hnsUpperBound) override;
    IFACEMETHODIMP ProcessEvent(DWORD dwInputStreamID, IMFMediaEvent* pEvent) override;
    IFACEMETHODIMP ProcessMessage(MFT_MESSAGE_TYPE eMessage, ULONG_PTR ulParam) override;
    IFACEMETHODIMP ProcessInput(DWORD dwInputStreamID, IMFSample* pSample, DWORD dwFlags) override;
    IFACEMETHODIMP ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount, MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
                                 DWORD* pdwStatus) override;

private:
    struct Frame {
        std::vector<BYTE> nv12;
        LONGLONG time = 0;
    };

    ~HevcDecoderMft();

    void CreateDecoder();
    void DestroyDecoder();
    void PushParameterSets();
    void PushNals(const BYTE* data, size_t size, LONGLONG pts);
    void Pump();
    void ClearQueue();
    HRESULT MakeOutputType(IMFMediaType** ppType);

    std::atomic<long> m_ref{1};
    std::mutex m_lock;

    IMFMediaType* m_inputType = nullptr;
    IMFMediaType* m_outputType = nullptr;
    IMFAttributes* m_attributes = nullptr;

    void* m_decoder = nullptr;  // de265_decoder_context*
    std::vector<std::vector<BYTE>> m_parameterSets;
    int m_nalLengthSize = 4;     // usato solo se i campioni non sono in formato Annex B

    UINT32 m_width = 0;
    UINT32 m_height = 0;
    LONGLONG m_frameDuration = 333667;  // 29.97 fps di default
    LONGLONG m_lastTime = 0;
    bool m_drained = false;

    std::deque<Frame> m_frames;
};
