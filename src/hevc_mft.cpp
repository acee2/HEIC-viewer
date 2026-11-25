#include "hevc_mft.h"
#include "module.h"
#include "log.h"

#include <mferror.h>
#include <libde265/de265.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <thread>

namespace {

de265_decoder_context* Ctx(void* p) { return static_cast<de265_decoder_context*>(p); }

int NalLayerId(const BYTE* nal, size_t len) {
    if (len < 2) return -1;
    return ((nal[0] & 0x01) << 5) | (nal[1] >> 3);
}

// True se il buffer è una sequenza di NAL con prefisso di lunghezza
// che copre esattamente tutto il buffer.
bool IsLengthPrefixed(const BYTE* p, size_t n, int lenSize) {
    size_t pos = 0;
    int count = 0;
    while (pos + lenSize <= n) {
        size_t len = 0;
        for (int i = 0; i < lenSize; ++i) len = (len << 8) | p[pos + i];
        pos += lenSize;
        if (len == 0 || pos + len > n) return false;
        pos += len;
        ++count;
    }
    return pos == n && count > 0;
}

template <class F>
void ForEachNalLengthPrefixed(const BYTE* p, size_t n, int lenSize, F&& fn) {
    size_t pos = 0;
    while (pos + lenSize <= n) {
        size_t len = 0;
        for (int i = 0; i < lenSize; ++i) len = (len << 8) | p[pos + i];
        pos += lenSize;
        if (len == 0 || pos + len > n) return;
        fn(p + pos, len);
        pos += len;
    }
}

template <class F>
void ForEachNalAnnexB(const BYTE* p, size_t n, F&& fn) {
    size_t i = 0;
    // Trova il primo start code.
    auto findStart = [&](size_t from) -> size_t {
        for (size_t k = from; k + 3 <= n; ++k) {
            if (p[k] == 0 && p[k + 1] == 0 && p[k + 2] == 1) return k;
        }
        return n;
    };
    i = findStart(0);
    while (i < n) {
        const size_t start = i + 3;
        size_t next = findStart(start);
        size_t end = next;
        while (end > start && p[end - 1] == 0) --end;  // zeri dello start code a 4 byte
        if (end > start) fn(p + start, end - start);
        i = next;
    }
}

void ParseHvcC(const BYTE* p, size_t n, std::vector<std::vector<BYTE>>& out, int& lenSize) {
    if (n < 23) return;
    lenSize = (p[21] & 0x03) + 1;
    const int arrays = p[22];
    size_t pos = 23;
    for (int a = 0; a < arrays; ++a) {
        if (pos + 3 > n) return;
        const int count = (p[pos + 1] << 8) | p[pos + 2];
        pos += 3;
        for (int i = 0; i < count; ++i) {
            if (pos + 2 > n) return;
            const size_t len = (size_t(p[pos]) << 8) | p[pos + 1];
            pos += 2;
            if (pos + len > n) return;
            out.emplace_back(p + pos, p + pos + len);
            pos += len;
        }
    }
}

// Copia un'immagine libde265 in un buffer NV12 di dimensione w x h.
void ToNv12(const de265_image* pic, UINT32 w, UINT32 h, std::vector<BYTE>& out) {
    out.assign(static_cast<size_t>(w) * h * 3 / 2, 0);
    BYTE* yDst = out.data();
    BYTE* uvDst = out.data() + static_cast<size_t>(w) * h;
    std::memset(yDst, 16, static_cast<size_t>(w) * h);
    std::memset(uvDst, 128, static_cast<size_t>(w) * h / 2);

    const int pw = de265_get_image_width(pic, 0);
    const int ph = de265_get_image_height(pic, 0);
    const int bpp = de265_get_bits_per_pixel(pic, 0);
    const int shift = bpp > 8 ? bpp - 8 : 0;
    const UINT32 cw = std::min<UINT32>(w, static_cast<UINT32>(pw));
    const UINT32 chh = std::min<UINT32>(h, static_cast<UINT32>(ph));

    int yStride = 0;
    const uint8_t* yPlane = de265_get_image_plane(pic, 0, &yStride);
    for (UINT32 y = 0; y < chh; ++y) {
        const uint8_t* src = yPlane + static_cast<size_t>(y) * yStride;
        BYTE* dst = yDst + static_cast<size_t>(y) * w;
        if (shift == 0) {
            std::memcpy(dst, src, cw);
        } else {
            const uint16_t* s16 = reinterpret_cast<const uint16_t*>(src);
            for (UINT32 x = 0; x < cw; ++x) dst[x] = static_cast<BYTE>(std::min<int>(255, s16[x] >> shift));
        }
    }

    if (de265_get_chroma_format(pic) == de265_chroma_mono) return;

    int uStride = 0, vStride = 0;
    const uint8_t* uPlane = de265_get_image_plane(pic, 1, &uStride);
    const uint8_t* vPlane = de265_get_image_plane(pic, 2, &vStride);
    const int pcw = de265_get_image_width(pic, 1);
    const int pch = de265_get_image_height(pic, 1);
    const int cbpp = de265_get_bits_per_pixel(pic, 1);
    const int cshift = cbpp > 8 ? cbpp - 8 : 0;

    const UINT32 outCw = cw / 2, outCh = chh / 2;
    for (UINT32 y = 0; y < outCh; ++y) {
        const int sy = std::min(pch - 1, static_cast<int>(static_cast<long long>(y) * 2 * pch / ph));
        BYTE* dst = uvDst + static_cast<size_t>(y) * w;
        for (UINT32 x = 0; x < outCw; ++x) {
            const int sx = std::min(pcw - 1, static_cast<int>(static_cast<long long>(x) * 2 * pcw / pw));
            int u, v;
            if (cshift == 0) {
                u = uPlane[static_cast<size_t>(sy) * uStride + sx];
                v = vPlane[static_cast<size_t>(sy) * vStride + sx];
            } else {
                u = reinterpret_cast<const uint16_t*>(uPlane + static_cast<size_t>(sy) * uStride)[sx] >> cshift;
                v = reinterpret_cast<const uint16_t*>(vPlane + static_cast<size_t>(sy) * vStride)[sx] >> cshift;
            }
            dst[x * 2] = static_cast<BYTE>(std::min(255, u));
            dst[x * 2 + 1] = static_cast<BYTE>(std::min(255, v));
        }
    }
}

template <class T>
void RotatePlane(const T* src, UINT32 w, UINT32 h, T* dst, UINT32 deg) {
    // deg in senso orario; dst ha dimensioni (h x w) per 90/270, (w x h) per 180.
    for (UINT32 y = 0; y < h; ++y) {
        for (UINT32 x = 0; x < w; ++x) {
            UINT32 nx, ny, nw;
            if (deg == 90) { nx = h - 1 - y; ny = x; nw = h; }
            else if (deg == 180) { nx = w - 1 - x; ny = h - 1 - y; nw = w; }
            else { nx = y; ny = w - 1 - x; nw = h; }
            dst[static_cast<size_t>(ny) * nw + nx] = src[static_cast<size_t>(y) * w + x];
        }
    }
}

// Ruota un fotogramma NV12 (w x h) in senso orario.
void RotateNv12(std::vector<BYTE>& frame, UINT32 w, UINT32 h, UINT32 deg) {
    if (deg != 90 && deg != 180 && deg != 270) return;
    std::vector<BYTE> out(frame.size());
    RotatePlane<BYTE>(frame.data(), w, h, out.data(), deg);
    const size_t ySize = static_cast<size_t>(w) * h;
    RotatePlane<uint16_t>(reinterpret_cast<const uint16_t*>(frame.data() + ySize), w / 2, h / 2,
                          reinterpret_cast<uint16_t*>(out.data() + ySize), deg);
    frame.swap(out);
}

}  // namespace

// ============================== Ciclo di vita ==============================

HevcDecoderMft::HevcDecoderMft() {
    ++g_dllRefs;
    MFCreateAttributes(&m_attributes, 2);
}

HevcDecoderMft::~HevcDecoderMft() {
    DestroyDecoder();
    if (m_inputType) m_inputType->Release();
    if (m_outputType) m_outputType->Release();
    if (m_attributes) m_attributes->Release();
    --g_dllRefs;
}

IFACEMETHODIMP HevcDecoderMft::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFTransform)) {
        *ppv = static_cast<IMFTransform*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) HevcDecoderMft::AddRef() { return static_cast<ULONG>(++m_ref); }

IFACEMETHODIMP_(ULONG) HevcDecoderMft::Release() {
    long r = --m_ref;
    if (r == 0) delete this;
    return static_cast<ULONG>(r);
}

void HevcDecoderMft::CreateDecoder() {
    DestroyDecoder();
    de265_decoder_context* ctx = de265_new_decoder();
    if (!ctx) return;
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    de265_start_worker_threads(ctx, static_cast<int>(std::min(cores, 16u)));
    m_decoder = ctx;
    PushParameterSets();
}

void HevcDecoderMft::DestroyDecoder() {
    if (m_decoder) {
        de265_free_decoder(Ctx(m_decoder));
        m_decoder = nullptr;
    }
}

void HevcDecoderMft::PushParameterSets() {
    if (!m_decoder) return;
    for (const auto& ps : m_parameterSets) {
        if (NalLayerId(ps.data(), ps.size()) == 0) {
            de265_push_NAL(Ctx(m_decoder), ps.data(), static_cast<int>(ps.size()), 0, nullptr);
        }
    }
}

void HevcDecoderMft::PushNals(const BYTE* data, size_t size, LONGLONG pts) {
    auto push = [&](const BYTE* nal, size_t len) {
        if (NalLayerId(nal, len) == 0) {
            de265_push_NAL(Ctx(m_decoder), nal, static_cast<int>(len), pts, nullptr);
        }
    };
    if (IsLengthPrefixed(data, size, m_nalLengthSize)) {
        ForEachNalLengthPrefixed(data, size, m_nalLengthSize, push);
    } else {
        ForEachNalAnnexB(data, size, push);
    }
}

// Fa lavorare il decoder e mette in coda i fotogrammi pronti.
void HevcDecoderMft::Pump() {
    if (!m_decoder) return;
    for (int guard = 0; guard < 100000; ++guard) {
        if (const de265_image* pic = de265_peek_next_picture(Ctx(m_decoder))) {
            Frame f;
            ToNv12(pic, m_srcWidth, m_srcHeight, f.nv12);
            RotateNv12(f.nv12, m_srcWidth, m_srcHeight, m_rotation);
            f.time = de265_get_image_PTS(pic);
            m_frames.push_back(std::move(f));
            de265_release_next_picture(Ctx(m_decoder));
            continue;
        }
        int more = 0;
        const de265_error err = de265_decode(Ctx(m_decoder), &more);
        if (err == DE265_ERROR_WAITING_FOR_INPUT_DATA) break;
        if (!more) {
            if (de265_peek_next_picture(Ctx(m_decoder))) continue;
            break;
        }
    }
}

void HevcDecoderMft::ClearQueue() { m_frames.clear(); }

// ============================== Stream ==============================

IFACEMETHODIMP HevcDecoderMft::GetStreamLimits(DWORD* inMin, DWORD* inMax, DWORD* outMin, DWORD* outMax) {
    if (!inMin || !inMax || !outMin || !outMax) return E_POINTER;
    *inMin = *inMax = *outMin = *outMax = 1;
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::GetStreamCount(DWORD* pcIn, DWORD* pcOut) {
    if (!pcIn || !pcOut) return E_POINTER;
    *pcIn = *pcOut = 1;
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::GetStreamIDs(DWORD, DWORD*, DWORD, DWORD*) { return E_NOTIMPL; }

IFACEMETHODIMP HevcDecoderMft::GetInputStreamInfo(DWORD id, MFT_INPUT_STREAM_INFO* info) {
    if (!info) return E_POINTER;
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    info->hnsMaxLatency = 0;
    info->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES;
    info->cbSize = 0;
    info->cbMaxLookahead = 0;
    info->cbAlignment = 0;
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::GetOutputStreamInfo(DWORD id, MFT_OUTPUT_STREAM_INFO* info) {
    if (!info) return E_POINTER;
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    std::lock_guard<std::mutex> guard(m_lock);
    info->dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
                    MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE | MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
    info->cbSize = m_width * m_height * 3 / 2;
    info->cbAlignment = 0;
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::GetAttributes(IMFAttributes** pp) {
    if (!pp) return E_POINTER;
    if (!m_attributes) return E_NOTIMPL;
    *pp = m_attributes;
    m_attributes->AddRef();
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::GetInputStreamAttributes(DWORD, IMFAttributes**) { return E_NOTIMPL; }
IFACEMETHODIMP HevcDecoderMft::GetOutputStreamAttributes(DWORD, IMFAttributes**) { return E_NOTIMPL; }
IFACEMETHODIMP HevcDecoderMft::DeleteInputStream(DWORD) { return E_NOTIMPL; }
IFACEMETHODIMP HevcDecoderMft::AddInputStreams(DWORD, DWORD*) { return E_NOTIMPL; }

// ============================== Tipi ==============================

IFACEMETHODIMP HevcDecoderMft::GetInputAvailableType(DWORD id, DWORD index, IMFMediaType** ppType) {
    if (!ppType) return E_POINTER;
    *ppType = nullptr;
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    const GUID* subtypes[] = {&MFVideoFormat_HEVC, &MFVideoFormat_HEVC_ES};
    if (index >= 2) return MF_E_NO_MORE_TYPES;

    IMFMediaType* type = nullptr;
    HRESULT hr = MFCreateMediaType(&type);
    if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_SUBTYPE, *subtypes[index]);
    if (SUCCEEDED(hr)) *ppType = type;
    else if (type) type->Release();
    return hr;
}

HRESULT HevcDecoderMft::MakeOutputType(IMFMediaType** ppType) {
    IMFMediaType* type = nullptr;
    HRESULT hr = MFCreateMediaType(&type);
    if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    if (SUCCEEDED(hr)) hr = MFSetAttributeSize(type, MF_MT_FRAME_SIZE, m_width, m_height);
    if (SUCCEEDED(hr)) hr = type->SetUINT32(MF_MT_DEFAULT_STRIDE, m_width);
    if (SUCCEEDED(hr)) hr = type->SetUINT32(MF_MT_SAMPLE_SIZE, m_width * m_height * 3 / 2);
    if (SUCCEEDED(hr)) hr = type->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
    if (SUCCEEDED(hr)) hr = type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    if (SUCCEEDED(hr)) hr = type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    if (SUCCEEDED(hr) && m_inputType) {
        UINT32 num = 0, den = 0;
        if (SUCCEEDED(MFGetAttributeRatio(m_inputType, MF_MT_FRAME_RATE, &num, &den)) && num && den)
            MFSetAttributeRatio(type, MF_MT_FRAME_RATE, num, den);
        if (SUCCEEDED(MFGetAttributeRatio(m_inputType, MF_MT_PIXEL_ASPECT_RATIO, &num, &den)) && num && den)
            MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, num, den);
        else
            MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    }
    // La rotazione la applichiamo noi: il lettore riceve il video già dritto.
    if (SUCCEEDED(hr)) type->SetUINT32(MF_MT_VIDEO_ROTATION, MFVideoRotationFormat_0);

    if (SUCCEEDED(hr)) *ppType = type;
    else if (type) type->Release();
    return hr;
}

IFACEMETHODIMP HevcDecoderMft::GetOutputAvailableType(DWORD id, DWORD index, IMFMediaType** ppType) {
    if (!ppType) return E_POINTER;
    *ppType = nullptr;
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    std::lock_guard<std::mutex> guard(m_lock);
    if (!m_inputType) return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (index > 0) return MF_E_NO_MORE_TYPES;
    return MakeOutputType(ppType);
}

IFACEMETHODIMP HevcDecoderMft::SetInputType(DWORD id, IMFMediaType* pType, DWORD dwFlags) {
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    std::lock_guard<std::mutex> guard(m_lock);

    if (!pType) {
        if (dwFlags & MFT_SET_TYPE_TEST_ONLY) return S_OK;
        if (m_inputType) { m_inputType->Release(); m_inputType = nullptr; }
        if (m_outputType) { m_outputType->Release(); m_outputType = nullptr; }
        DestroyDecoder();
        ClearQueue();
        return S_OK;
    }

    GUID major = GUID_NULL, subtype = GUID_NULL;
    if (FAILED(pType->GetGUID(MF_MT_MAJOR_TYPE, &major)) || major != MFMediaType_Video)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(pType->GetGUID(MF_MT_SUBTYPE, &subtype)) ||
        (subtype != MFVideoFormat_HEVC && subtype != MFVideoFormat_HEVC_ES))
        return MF_E_INVALIDMEDIATYPE;

    UINT32 w = 0, h = 0;
    if (FAILED(MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &w, &h)) || w == 0 || h == 0 || w > 16384 || h > 16384) {
        LogError(L"Decoder video: tipo HEVC senza dimensioni", MF_E_INVALIDMEDIATYPE);
        return MF_E_INVALIDMEDIATYPE;
    }

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY) return S_OK;

    if (m_inputType) m_inputType->Release();
    m_inputType = pType;
    m_inputType->AddRef();
    if (m_outputType) { m_outputType->Release(); m_outputType = nullptr; }

    m_srcWidth = (w + 1) & ~1u;
    m_srcHeight = (h + 1) & ~1u;

    // MF_MT_VIDEO_ROTATION = gradi in senso antiorario di cui il contenuto è ruotato:
    // per raddrizzarlo lo giriamo dello stesso valore in senso orario.
    UINT32 rot = 0;
    m_rotation = SUCCEEDED(pType->GetUINT32(MF_MT_VIDEO_ROTATION, &rot)) ? (rot % 360) : 0;
    if (m_rotation != 90 && m_rotation != 180 && m_rotation != 270) m_rotation = 0;
    const bool swap = (m_rotation == 90 || m_rotation == 270);
    m_width = swap ? m_srcHeight : m_srcWidth;
    m_height = swap ? m_srcWidth : m_srcHeight;

    UINT32 num = 0, den = 0;
    if (SUCCEEDED(MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den)) && num && den)
        m_frameDuration = static_cast<LONGLONG>(10000000ll * den / num);

    // Parametri del codec (VPS/SPS/PPS), in formato Annex B oppure hvcC.
    m_parameterSets.clear();
    m_nalLengthSize = 4;
    UINT32 blobSize = 0;
    if (SUCCEEDED(pType->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &blobSize)) && blobSize > 0) {
        std::vector<BYTE> blob(blobSize);
        if (SUCCEEDED(pType->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER, blob.data(), blobSize, nullptr))) {
            if (blob.size() >= 23 && blob[0] == 1) {
                ParseHvcC(blob.data(), blob.size(), m_parameterSets, m_nalLengthSize);
            } else {
                ForEachNalAnnexB(blob.data(), blob.size(), [&](const BYTE* nal, size_t len) {
                    m_parameterSets.emplace_back(nal, nal + len);
                });
            }
        }
    }

    ClearQueue();
    CreateDecoder();
    if (!m_decoder) LogError(L"Decoder video: libde265 non avviato", E_OUTOFMEMORY);
    if (m_parameterSets.empty()) LogError(L"Decoder video: mancano VPS/SPS/PPS nel tipo di ingresso", S_FALSE);
    return m_decoder ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP HevcDecoderMft::SetOutputType(DWORD id, IMFMediaType* pType, DWORD dwFlags) {
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    std::lock_guard<std::mutex> guard(m_lock);

    if (!pType) {
        if (dwFlags & MFT_SET_TYPE_TEST_ONLY) return S_OK;
        if (m_outputType) { m_outputType->Release(); m_outputType = nullptr; }
        return S_OK;
    }
    if (!m_inputType) return MF_E_TRANSFORM_TYPE_NOT_SET;

    GUID major = GUID_NULL, subtype = GUID_NULL;
    if (FAILED(pType->GetGUID(MF_MT_MAJOR_TYPE, &major)) || major != MFMediaType_Video)
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(pType->GetGUID(MF_MT_SUBTYPE, &subtype)) || subtype != MFVideoFormat_NV12)
        return MF_E_INVALIDMEDIATYPE;
    UINT32 w = 0, h = 0;
    if (FAILED(MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &w, &h)) || w != m_width || h != m_height)
        return MF_E_INVALIDMEDIATYPE;

    if (dwFlags & MFT_SET_TYPE_TEST_ONLY) return S_OK;

    if (m_outputType) m_outputType->Release();
    m_outputType = pType;
    m_outputType->AddRef();
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::GetInputCurrentType(DWORD id, IMFMediaType** ppType) {
    if (!ppType) return E_POINTER;
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    std::lock_guard<std::mutex> guard(m_lock);
    if (!m_inputType) return MF_E_TRANSFORM_TYPE_NOT_SET;
    *ppType = m_inputType;
    m_inputType->AddRef();
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::GetOutputCurrentType(DWORD id, IMFMediaType** ppType) {
    if (!ppType) return E_POINTER;
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    std::lock_guard<std::mutex> guard(m_lock);
    if (!m_outputType) return MF_E_TRANSFORM_TYPE_NOT_SET;
    *ppType = m_outputType;
    m_outputType->AddRef();
    return S_OK;
}

// ============================== Elaborazione ==============================

IFACEMETHODIMP HevcDecoderMft::GetInputStatus(DWORD id, DWORD* pdwFlags) {
    if (!pdwFlags) return E_POINTER;
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    std::lock_guard<std::mutex> guard(m_lock);
    *pdwFlags = m_frames.empty() ? MFT_INPUT_STATUS_ACCEPT_DATA : 0;
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::GetOutputStatus(DWORD* pdwFlags) {
    if (!pdwFlags) return E_POINTER;
    std::lock_guard<std::mutex> guard(m_lock);
    *pdwFlags = m_frames.empty() ? 0 : MFT_OUTPUT_STATUS_SAMPLE_READY;
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::SetOutputBounds(LONGLONG, LONGLONG) { return E_NOTIMPL; }
IFACEMETHODIMP HevcDecoderMft::ProcessEvent(DWORD, IMFMediaEvent*) { return E_NOTIMPL; }

IFACEMETHODIMP HevcDecoderMft::ProcessMessage(MFT_MESSAGE_TYPE msg, ULONG_PTR) {
    std::lock_guard<std::mutex> guard(m_lock);
    switch (msg) {
        case MFT_MESSAGE_COMMAND_FLUSH:
            // Dopo un salto (seek) si riparte con un decoder pulito.
            ClearQueue();
            if (m_inputType) CreateDecoder();
            m_drained = false;
            return S_OK;
        case MFT_MESSAGE_COMMAND_DRAIN:
            if (m_decoder) {
                de265_flush_data(Ctx(m_decoder));
                Pump();
                m_drained = true;
            }
            return S_OK;
        case MFT_MESSAGE_SET_D3D_MANAGER:
            return E_NOTIMPL;
        default:
            return S_OK;
    }
}

IFACEMETHODIMP HevcDecoderMft::ProcessInput(DWORD id, IMFSample* pSample, DWORD) {
    if (!pSample) return E_POINTER;
    if (id != 0) return MF_E_INVALIDSTREAMNUMBER;
    std::lock_guard<std::mutex> guard(m_lock);
    if (!m_inputType || !m_outputType) return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (!m_frames.empty()) return MF_E_NOTACCEPTING;
    if (m_drained) {  // dopo la fine del flusso il decoder va ricreato
        CreateDecoder();
        m_drained = false;
    }
    if (!m_decoder) return E_UNEXPECTED;

    LONGLONG time = 0;
    if (FAILED(pSample->GetSampleTime(&time))) time = m_lastTime + m_frameDuration;
    m_lastTime = time;

    IMFMediaBuffer* buffer = nullptr;
    HRESULT hr = pSample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr)) return hr;

    BYTE* data = nullptr;
    DWORD length = 0;
    hr = buffer->Lock(&data, nullptr, &length);
    if (SUCCEEDED(hr)) {
        PushNals(data, length, time);
        buffer->Unlock();
    }
    buffer->Release();
    if (FAILED(hr)) return hr;

    Pump();
    return S_OK;
}

IFACEMETHODIMP HevcDecoderMft::ProcessOutput(DWORD, DWORD count, MFT_OUTPUT_DATA_BUFFER* out, DWORD* pdwStatus) {
    if (!out || !pdwStatus || count != 1) return E_INVALIDARG;
    *pdwStatus = 0;
    out[0].dwStatus = 0;
    out[0].pEvents = nullptr;

    std::lock_guard<std::mutex> guard(m_lock);
    if (!m_inputType || !m_outputType) return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (m_frames.empty()) Pump();
    if (m_frames.empty()) return MF_E_TRANSFORM_NEED_MORE_INPUT;

    Frame frame = std::move(m_frames.front());
    m_frames.pop_front();

    IMFMediaBuffer* buffer = nullptr;
    IMFSample* sample = nullptr;
    const DWORD size = static_cast<DWORD>(frame.nv12.size());

    HRESULT hr = MFCreateMemoryBuffer(size, &buffer);
    if (SUCCEEDED(hr)) {
        BYTE* dst = nullptr;
        hr = buffer->Lock(&dst, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            std::memcpy(dst, frame.nv12.data(), size);
            buffer->Unlock();
            hr = buffer->SetCurrentLength(size);
        }
    }
    if (SUCCEEDED(hr)) hr = MFCreateSample(&sample);
    if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer);
    if (SUCCEEDED(hr)) hr = sample->SetSampleTime(frame.time);
    if (SUCCEEDED(hr)) hr = sample->SetSampleDuration(m_frameDuration);
    if (SUCCEEDED(hr)) sample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);

    if (buffer) buffer->Release();
    if (FAILED(hr)) {
        if (sample) sample->Release();
        return hr;
    }

    out[0].pSample = sample;  // il riferimento passa al chiamante
    if (!m_frames.empty()) out[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_INCOMPLETE;
    return S_OK;
}
