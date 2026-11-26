#include "video_decode.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wincodec.h>
#include <libde265/de265.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr uint64_t kMaxMoovSize = 64ull * 1024 * 1024;
constexpr uint32_t kMaxSampleSize = 64u * 1024 * 1024;

constexpr uint32_t FourCC(const char (&s)[5]) {
    return (uint32_t(uint8_t(s[0])) << 24) | (uint32_t(uint8_t(s[1])) << 16) | (uint32_t(uint8_t(s[2])) << 8) |
           uint32_t(uint8_t(s[3]));
}

uint16_t BE16(const BYTE* p) { return uint16_t((p[0] << 8) | p[1]); }
uint32_t BE32(const BYTE* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint64_t BE64(const BYTE* p) { return (uint64_t(BE32(p)) << 32) | BE32(p + 4); }

// ---------------------------------------------------------------------------
// Lettura dello stream
// ---------------------------------------------------------------------------

HRESULT ReadAt(IStream* s, uint64_t offset, void* buffer, ULONG size) {
    LARGE_INTEGER pos;
    pos.QuadPart = static_cast<LONGLONG>(offset);
    HRESULT hr = s->Seek(pos, STREAM_SEEK_SET, nullptr);
    if (FAILED(hr)) return hr;
    ULONG total = 0;
    BYTE* out = static_cast<BYTE*>(buffer);
    while (total < size) {
        ULONG read = 0;
        hr = s->Read(out + total, size - total, &read);
        if (FAILED(hr)) return hr;
        if (read == 0) return E_FAIL;
        total += read;
    }
    return S_OK;
}

uint64_t StreamSize(IStream* s) {
    STATSTG st = {};
    if (SUCCEEDED(s->Stat(&st, STATFLAG_NONAME))) return st.cbSize.QuadPart;
    return 0;
}

// Cerca il box "moov" al primo livello del file e lo legge in memoria.
HRESULT ReadMoov(IStream* s, std::vector<BYTE>& moov) {
    const uint64_t fileSize = StreamSize(s);
    if (fileSize < 16) return E_FAIL;

    uint64_t offset = 0;
    while (offset + 8 <= fileSize) {
        BYTE header[16];
        HRESULT hr = ReadAt(s, offset, header, 8);
        if (FAILED(hr)) return hr;
        uint64_t size = BE32(header);
        const uint32_t type = BE32(header + 4);
        uint64_t headerSize = 8;
        if (size == 1) {
            hr = ReadAt(s, offset + 8, header + 8, 8);
            if (FAILED(hr)) return hr;
            size = BE64(header + 8);
            headerSize = 16;
        } else if (size == 0) {
            size = fileSize - offset;
        }
        if (size < headerSize || offset + size > fileSize) return E_FAIL;

        if (type == FourCC("moov")) {
            const uint64_t payload = size - headerSize;
            if (payload == 0 || payload > kMaxMoovSize) return E_FAIL;
            moov.resize(static_cast<size_t>(payload));
            return ReadAt(s, offset + headerSize, moov.data(), static_cast<ULONG>(payload));
        }
        offset += size;
    }
    return E_FAIL;
}

// ---------------------------------------------------------------------------
// Parser MP4/MOV (solo quello che serve per trovare un fotogramma)
// ---------------------------------------------------------------------------

template <class F>
void ForEachBox(const BYTE* p, size_t n, F&& fn) {
    size_t pos = 0;
    while (pos + 8 <= n) {
        uint64_t size = BE32(p + pos);
        const uint32_t type = BE32(p + pos + 4);
        size_t headerSize = 8;
        if (size == 1) {
            if (pos + 16 > n) return;
            size = BE64(p + pos + 8);
            headerSize = 16;
        } else if (size == 0) {
            size = n - pos;
        }
        if (size < headerSize || size > n - pos) return;
        fn(type, p + pos + headerSize, static_cast<size_t>(size - headerSize));
        pos += static_cast<size_t>(size);
    }
}

struct StscEntry {
    uint32_t firstChunk;
    uint32_t samplesPerChunk;
};

struct Track {
    bool isVideo = false;
    uint32_t codec = 0;
    int nalLengthSize = 4;
    int rotation = 0;
    std::vector<std::vector<BYTE>> parameterSets;  // VPS/SPS/PPS (HEVC)
    uint32_t fixedSampleSize = 0;
    uint32_t sampleCount = 0;
    std::vector<uint32_t> sampleSizes;
    std::vector<uint64_t> chunkOffsets;
    std::vector<StscEntry> stsc;
    std::vector<uint32_t> syncSamples;

    bool IsHevc() const {
        return codec == FourCC("hvc1") || codec == FourCC("hev1") || codec == FourCC("dvh1") ||
               codec == FourCC("dvhe");
    }
};

void ParseHvcC(const BYTE* p, size_t n, Track& t) {
    if (n < 23) return;
    t.nalLengthSize = (p[21] & 0x03) + 1;
    const int arrays = p[22];
    size_t pos = 23;
    for (int a = 0; a < arrays; ++a) {
        if (pos + 3 > n) return;
        const int count = BE16(p + pos + 1);
        pos += 3;
        for (int i = 0; i < count; ++i) {
            if (pos + 2 > n) return;
            const size_t len = BE16(p + pos);
            pos += 2;
            if (pos + len > n) return;
            t.parameterSets.emplace_back(p + pos, p + pos + len);
            pos += len;
        }
    }
}

void ParseStsd(const BYTE* p, size_t n, Track& t) {
    if (n < 8) return;
    // Prima "sample entry" dopo version/flags + entry_count.
    ForEachBox(p + 8, n - 8, [&](uint32_t type, const BYTE* entry, size_t len) {
        if (t.codec != 0) return;
        t.codec = type;
        constexpr size_t kVisualSampleEntrySize = 78;
        if (len <= kVisualSampleEntrySize) return;
        ForEachBox(entry + kVisualSampleEntrySize, len - kVisualSampleEntrySize,
                   [&](uint32_t childType, const BYTE* child, size_t childLen) {
                       if (childType == FourCC("hvcC")) ParseHvcC(child, childLen, t);
                   });
    });
}

void ParseStbl(const BYTE* p, size_t n, Track& t) {
    ForEachBox(p, n, [&](uint32_t type, const BYTE* b, size_t len) {
        if (type == FourCC("stsd")) {
            ParseStsd(b, len, t);
        } else if (type == FourCC("stsz") && len >= 12) {
            t.fixedSampleSize = BE32(b + 4);
            t.sampleCount = BE32(b + 8);
            if (t.fixedSampleSize == 0) {
                const size_t count = std::min<size_t>(t.sampleCount, (len - 12) / 4);
                t.sampleSizes.resize(count);
                for (size_t i = 0; i < count; ++i) t.sampleSizes[i] = BE32(b + 12 + i * 4);
            }
        } else if (type == FourCC("stsc") && len >= 8) {
            const size_t count = std::min<size_t>(BE32(b + 4), (len - 8) / 12);
            for (size_t i = 0; i < count; ++i) {
                const BYTE* e = b + 8 + i * 12;
                t.stsc.push_back({BE32(e), BE32(e + 4)});
            }
        } else if (type == FourCC("stco") && len >= 8) {
            const size_t count = std::min<size_t>(BE32(b + 4), (len - 8) / 4);
            for (size_t i = 0; i < count; ++i) t.chunkOffsets.push_back(BE32(b + 8 + i * 4));
        } else if (type == FourCC("co64") && len >= 8) {
            const size_t count = std::min<size_t>(BE32(b + 4), (len - 8) / 8);
            for (size_t i = 0; i < count; ++i) t.chunkOffsets.push_back(BE64(b + 8 + i * 8));
        } else if (type == FourCC("stss") && len >= 8) {
            const size_t count = std::min<size_t>(BE32(b + 4), (len - 8) / 4);
            for (size_t i = 0; i < count; ++i) t.syncSamples.push_back(BE32(b + 8 + i * 4));
        }
    });
}

int RotationFromMatrix(const BYTE* m) {
    const int32_t a = static_cast<int32_t>(BE32(m));
    const int32_t b = static_cast<int32_t>(BE32(m + 4));
    const int32_t c = static_cast<int32_t>(BE32(m + 12));
    const int32_t d = static_cast<int32_t>(BE32(m + 16));
    if (a == 0 && b > 0 && c < 0) return 90;
    if (a < 0 && d < 0 && b == 0 && c == 0) return 180;
    if (a == 0 && b < 0 && c > 0) return 270;
    return 0;
}

void ParseTkhd(const BYTE* p, size_t n, Track& t) {
    if (n < 4) return;
    const size_t matrixOffset = (p[0] == 1) ? 52 : 40;
    if (n >= matrixOffset + 36) t.rotation = RotationFromMatrix(p + matrixOffset);
}

void ParseTrak(const BYTE* p, size_t n, Track& t) {
    ForEachBox(p, n, [&](uint32_t type, const BYTE* b, size_t len) {
        if (type == FourCC("tkhd")) {
            ParseTkhd(b, len, t);
        } else if (type == FourCC("mdia")) {
            ForEachBox(b, len, [&](uint32_t mt, const BYTE* mb, size_t ml) {
                if (mt == FourCC("hdlr") && ml >= 12) {
                    t.isVideo = BE32(mb + 8) == FourCC("vide");
                } else if (mt == FourCC("minf")) {
                    ForEachBox(mb, ml, [&](uint32_t it, const BYTE* ib, size_t il) {
                        if (it == FourCC("stbl")) ParseStbl(ib, il, t);
                    });
                }
            });
        }
    });
}

bool FindVideoTrack(const std::vector<BYTE>& moov, Track& out) {
    bool found = false;
    ForEachBox(moov.data(), moov.size(), [&](uint32_t type, const BYTE* b, size_t len) {
        if (found || type != FourCC("trak")) return;
        Track t;
        ParseTrak(b, len, t);
        if (t.isVideo && t.sampleCount > 0) {
            out = std::move(t);
            found = true;
        }
    });
    return found;
}

uint32_t SampleSize(const Track& t, uint32_t sample) {
    if (t.fixedSampleSize) return t.fixedSampleSize;
    return (sample >= 1 && sample <= t.sampleSizes.size()) ? t.sampleSizes[sample - 1] : 0;
}

// Posizione nel file del campione n (numerato da 1).
bool SampleLocation(const Track& t, uint32_t sample, uint64_t& offset, uint32_t& size) {
    if (sample == 0 || sample > t.sampleCount || t.chunkOffsets.empty()) return false;
    const uint64_t numChunks = t.chunkOffsets.size();
    uint64_t firstSampleOfRun = 1;

    for (size_t i = 0; i < t.stsc.size(); ++i) {
        const uint64_t c0 = t.stsc[i].firstChunk;
        const uint64_t c1 = (i + 1 < t.stsc.size()) ? t.stsc[i + 1].firstChunk : numChunks + 1;
        const uint64_t per = t.stsc[i].samplesPerChunk;
        if (per == 0 || c0 == 0 || c1 <= c0) continue;

        const uint64_t runSamples = (c1 - c0) * per;
        if (sample < firstSampleOfRun + runSamples) {
            const uint64_t index = sample - firstSampleOfRun;
            const uint64_t chunk = c0 + index / per;
            if (chunk > numChunks) return false;
            const uint64_t firstInChunk = firstSampleOfRun + (index / per) * per;

            offset = t.chunkOffsets[static_cast<size_t>(chunk - 1)];
            for (uint64_t s = firstInChunk; s < sample; ++s) offset += SampleSize(t, static_cast<uint32_t>(s));
            size = SampleSize(t, sample);
            return size > 0;
        }
        firstSampleOfRun += runSamples;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Decodifica HEVC con libde265
// ---------------------------------------------------------------------------

int NalLayerId(const BYTE* nal, size_t len) {
    if (len < 2) return -1;
    return ((nal[0] & 0x01) << 5) | (nal[1] >> 3);
}

void ConvertYuvToBgra(const de265_image* pic, DecodedImage& img) {
    const int w = de265_get_image_width(pic, 0);
    const int h = de265_get_image_height(pic, 0);
    const int bpp = de265_get_bits_per_pixel(pic, 0);
    const de265_chroma chroma = de265_get_chroma_format(pic);

    int yStride = 0, uStride = 0, vStride = 0;
    const uint8_t* yPlane = de265_get_image_plane(pic, 0, &yStride);
    const uint8_t* uPlane = chroma != de265_chroma_mono ? de265_get_image_plane(pic, 1, &uStride) : nullptr;
    const uint8_t* vPlane = chroma != de265_chroma_mono ? de265_get_image_plane(pic, 2, &vStride) : nullptr;
    const int cw = uPlane ? de265_get_image_width(pic, 1) : 1;
    const int ch = uPlane ? de265_get_image_height(pic, 1) : 1;

    img.width = static_cast<UINT>(w);
    img.height = static_cast<UINT>(h);
    img.bgra.resize(static_cast<size_t>(w) * h * 4);

    // 8 bit -> BT.709 (SDR); 10 bit (iPhone HDR) -> BT.2020. Range limitato.
    const bool wide = bpp > 8;
    const float kr = wide ? 1.4746f : 1.5748f;
    const float kgb = wide ? 0.16455f : 0.1873f;
    const float kgr = wide ? 0.57135f : 0.4681f;
    const float kb = wide ? 1.8814f : 1.8556f;
    const float scale = static_cast<float>(1 << (bpp - 8));

    auto sample = [bpp](const uint8_t* plane, int stride, int x, int y) -> float {
        if (bpp > 8) return static_cast<float>(reinterpret_cast<const uint16_t*>(plane + static_cast<size_t>(y) * stride)[x]);
        return static_cast<float>(plane[static_cast<size_t>(y) * stride + x]);
    };
    auto clamp8 = [](float v) -> BYTE { return static_cast<BYTE>(std::min(255.0f, std::max(0.0f, v * 255.0f + 0.5f))); };

    for (int y = 0; y < h; ++y) {
        BYTE* dst = img.bgra.data() + static_cast<size_t>(y) * w * 4;
        const int cy = uPlane ? std::min(ch - 1, y * ch / h) : 0;
        for (int x = 0; x < w; ++x, dst += 4) {
            const float Y = (sample(yPlane, yStride, x, y) / scale - 16.0f) / 219.0f;
            float Cb = 0.0f, Cr = 0.0f;
            if (uPlane) {
                const int cx = std::min(cw - 1, x * cw / w);
                Cb = (sample(uPlane, uStride, cx, cy) / scale - 128.0f) / 224.0f;
                Cr = (sample(vPlane, vStride, cx, cy) / scale - 128.0f) / 224.0f;
            }
            dst[0] = clamp8(Y + kb * Cb);
            dst[1] = clamp8(Y - kgb * Cb - kgr * Cr);
            dst[2] = clamp8(Y + kr * Cr);
            dst[3] = 0xFF;
        }
    }
}

HRESULT DecodeHevcFrame(const Track& t, const std::vector<BYTE>& sampleData, DecodedImage& img) {
    de265_decoder_context* ctx = de265_new_decoder();
    if (!ctx) return E_OUTOFMEMORY;

    for (const auto& ps : t.parameterSets) {
        if (NalLayerId(ps.data(), ps.size()) == 0) {
            de265_push_NAL(ctx, ps.data(), static_cast<int>(ps.size()), 0, nullptr);
        }
    }

    // Il campione è una sequenza di NAL con prefisso di lunghezza.
    const size_t lenSize = static_cast<size_t>(t.nalLengthSize);
    size_t pos = 0;
    while (pos + lenSize <= sampleData.size()) {
        size_t len = 0;
        for (size_t i = 0; i < lenSize; ++i) len = (len << 8) | sampleData[pos + i];
        pos += lenSize;
        if (len == 0 || pos + len > sampleData.size()) break;
        // Solo il livello base (i video "spaziali" hanno un secondo livello).
        if (NalLayerId(sampleData.data() + pos, len) == 0) {
            de265_push_NAL(ctx, sampleData.data() + pos, static_cast<int>(len), 0, nullptr);
        }
        pos += len;
    }
    de265_flush_data(ctx);

    HRESULT hr = WINCODEC_ERR_BADIMAGE;
    for (int guard = 0; guard < 10000; ++guard) {
        int more = 0;
        const de265_error err = de265_decode(ctx, &more);

        if (const de265_image* pic = de265_peek_next_picture(ctx)) {
            ConvertYuvToBgra(pic, img);
            de265_release_next_picture(ctx);
            hr = S_OK;
            break;
        }
        if (!more || (err != DE265_OK && !de265_isOK(err))) break;
    }

    de265_free_decoder(ctx);
    return hr;
}

// ---------------------------------------------------------------------------
// Ripiego: Media Foundation (H.264 e altri codec già supportati da Windows)
// ---------------------------------------------------------------------------

HRESULT DecodeWithMediaFoundation(IStream* stream, DecodedImage& img) {
    LARGE_INTEGER zero = {};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) return hr;

    IMFByteStream* byteStream = nullptr;
    IMFAttributes* attrs = nullptr;
    IMFSourceReader* reader = nullptr;
    IMFMediaType* type = nullptr;
    IMFSample* sample = nullptr;
    IMFMediaBuffer* buffer = nullptr;
    const DWORD videoStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

    hr = MFCreateMFByteStreamOnStream(stream, &byteStream);
    if (SUCCEEDED(hr)) hr = MFCreateAttributes(&attrs, 1);
    if (SUCCEEDED(hr)) hr = attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromByteStream(byteStream, attrs, &reader);
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&type);
    if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (SUCCEEDED(hr)) hr = reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    if (SUCCEEDED(hr)) hr = reader->SetStreamSelection(videoStream, TRUE);
    if (SUCCEEDED(hr)) hr = reader->SetCurrentMediaType(videoStream, nullptr, type);
    if (type) { type->Release(); type = nullptr; }

    for (int tries = 0; SUCCEEDED(hr) && !sample && tries < 60; ++tries) {
        DWORD index = 0, flags = 0;
        LONGLONG ts = 0;
        hr = reader->ReadSample(videoStream, 0, &index, &flags, &ts, &sample);
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
    }
    if (SUCCEEDED(hr) && !sample) hr = E_FAIL;

    UINT32 w = 0, h = 0;
    LONG stride = 0;
    if (SUCCEEDED(hr)) hr = reader->GetCurrentMediaType(videoStream, &type);
    if (SUCCEEDED(hr)) hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &w, &h);
    if (SUCCEEDED(hr)) {
        UINT32 s = 0;
        stride = SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &s)) ? static_cast<LONG>(static_cast<INT32>(s))
                                                                      : static_cast<LONG>(w * 4);
    }
    if (SUCCEEDED(hr)) hr = sample->ConvertToContiguousBuffer(&buffer);

    if (SUCCEEDED(hr) && w > 0 && h > 0) {
        BYTE* data = nullptr;
        DWORD length = 0;
        hr = buffer->Lock(&data, nullptr, &length);
        if (SUCCEEDED(hr)) {
            const size_t absStride = static_cast<size_t>(stride < 0 ? -stride : stride);
            if (absStride >= w * 4ull && length >= absStride * h) {
                img.width = w;
                img.height = h;
                img.bgra.resize(static_cast<size_t>(w) * h * 4);
                for (UINT32 y = 0; y < h; ++y) {
                    const BYTE* src = stride < 0 ? data + absStride * (h - 1 - y) : data + absStride * y;
                    BYTE* dst = img.bgra.data() + static_cast<size_t>(y) * w * 4;
                    for (UINT32 x = 0; x < w; ++x) {
                        dst[x * 4 + 0] = src[x * 4 + 0];
                        dst[x * 4 + 1] = src[x * 4 + 1];
                        dst[x * 4 + 2] = src[x * 4 + 2];
                        dst[x * 4 + 3] = 0xFF;
                    }
                }
            } else {
                hr = E_FAIL;
            }
            buffer->Unlock();
        }
    } else if (SUCCEEDED(hr)) {
        hr = E_FAIL;
    }

    if (buffer) buffer->Release();
    if (sample) sample->Release();
    if (type) type->Release();
    if (reader) reader->Release();
    if (attrs) attrs->Release();
    if (byteStream) byteStream->Release();
    MFShutdown();
    return hr;
}

}  // namespace

void RotateBGRA(DecodedImage& img, int degreesClockwise) {
    const int deg = ((degreesClockwise % 360) + 360) % 360;
    if (deg != 90 && deg != 180 && deg != 270) return;

    const UINT w = img.width, h = img.height;
    std::vector<BYTE> out(img.bgra.size());
    const UINT nw = (deg == 180) ? w : h;
    const UINT nh = (deg == 180) ? h : w;

    for (UINT y = 0; y < h; ++y) {
        for (UINT x = 0; x < w; ++x) {
            UINT nx, ny;
            if (deg == 90) { nx = h - 1 - y; ny = x; }
            else if (deg == 180) { nx = w - 1 - x; ny = h - 1 - y; }
            else { nx = y; ny = w - 1 - x; }
            std::memcpy(&out[(static_cast<size_t>(ny) * nw + nx) * 4], &img.bgra[(static_cast<size_t>(y) * w + x) * 4], 4);
        }
    }
    img.bgra.swap(out);
    img.width = nw;
    img.height = nh;
}

HRESULT DecodeVideoFrame(IStream* stream, DecodedImage& img) {
    if (!stream) return E_POINTER;

    Track track;
    std::vector<BYTE> moov;
    const bool parsed = SUCCEEDED(ReadMoov(stream, moov)) && FindVideoTrack(moov, track);

    HRESULT hr = E_FAIL;
    if (parsed && track.IsHevc() && !track.parameterSets.empty()) {
        uint32_t sampleNumber = track.syncSamples.empty() ? 1 : track.syncSamples.front();
        uint64_t offset = 0;
        uint32_t size = 0;
        if (SampleLocation(track, sampleNumber, offset, size) && size <= kMaxSampleSize) {
            std::vector<BYTE> data(size);
            if (SUCCEEDED(ReadAt(stream, offset, data.data(), size))) {
                hr = DecodeHevcFrame(track, data, img);
            }
        }
    }

    if (FAILED(hr)) {
        img = DecodedImage();
        hr = DecodeWithMediaFoundation(stream, img);
    }
    if (SUCCEEDED(hr) && parsed) RotateBGRA(img, track.rotation);
    return hr;
}
