#include "heic_decode.h"

#include <wincodec.h>
#include <libheif/heif.h>

#include <algorithm>
#include <cstring>
#include <mutex>

namespace {

void EnsureLibheifInit() {
    static std::once_flag flag;
    std::call_once(flag, [] { heif_init(nullptr); });
}

const char* const kBrands[] = {"heic", "heix", "hevc", "hevx", "heim", "heis", "mif1", "msf1"};

}  // namespace

HRESULT ReadAllFromStream(IStream* stream, std::vector<BYTE>& out) {
    if (!stream) return E_POINTER;
    out.clear();

    STATSTG st = {};
    if (SUCCEEDED(stream->Stat(&st, STATFLAG_NONAME)) && st.cbSize.QuadPart > 0 &&
        st.cbSize.QuadPart < (1ull << 31)) {
        out.reserve(static_cast<size_t>(st.cbSize.QuadPart));
    }

    std::vector<BYTE> chunk(1 << 16);
    for (;;) {
        ULONG read = 0;
        HRESULT hr = stream->Read(chunk.data(), static_cast<ULONG>(chunk.size()), &read);
        if (FAILED(hr)) return hr;
        if (read == 0) break;
        out.insert(out.end(), chunk.begin(), chunk.begin() + read);
        if (hr == S_FALSE) break;
    }
    return out.empty() ? E_FAIL : S_OK;
}

bool LooksLikeHeic(const BYTE* data, size_t size) {
    if (!data || size < 12) return false;
    if (std::memcmp(data + 4, "ftyp", 4) != 0) return false;
    for (const char* brand : kBrands) {
        if (std::memcmp(data + 8, brand, 4) == 0) return true;
    }
    return false;
}

HRESULT DecodeHeic(const BYTE* data, size_t size, DecodedImage& img, UINT preferThumbAtLeast) {
    if (!data || size == 0) return E_INVALIDARG;
    EnsureLibheifInit();

    heif_context* ctx = heif_context_alloc();
    if (!ctx) return E_OUTOFMEMORY;

    HRESULT hr = WINCODEC_ERR_BADIMAGE;
    heif_image_handle* primary = nullptr;
    heif_image_handle* use = nullptr;
    heif_image* image = nullptr;

    do {
        heif_error err = heif_context_read_from_memory_without_copy(ctx, data, size, nullptr);
        if (err.code != heif_error_Ok) { hr = WINCODEC_ERR_BADHEADER; break; }

        err = heif_context_get_primary_image_handle(ctx, &primary);
        if (err.code != heif_error_Ok || !primary) break;
        use = primary;

        // Per le miniature: usa quella incorporata se è abbastanza grande.
        if (preferThumbAtLeast > 0) {
            int count = heif_image_handle_get_number_of_thumbnails(primary);
            if (count > 0) {
                std::vector<heif_item_id> ids(static_cast<size_t>(count));
                count = heif_image_handle_get_list_of_thumbnail_IDs(primary, ids.data(), count);
                for (int i = 0; i < count; ++i) {
                    heif_image_handle* thumb = nullptr;
                    if (heif_image_handle_get_thumbnail(primary, ids[i], &thumb).code != heif_error_Ok || !thumb)
                        continue;
                    int tw = heif_image_handle_get_width(thumb);
                    int th = heif_image_handle_get_height(thumb);
                    if (static_cast<UINT>(std::max(tw, th)) >= preferThumbAtLeast) {
                        use = thumb;
                        break;
                    }
                    heif_image_handle_release(thumb);
                }
            }
        }

        const bool hasAlpha = heif_image_handle_has_alpha_channel(use) != 0;

        err = heif_decode_image(use, &image, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
        if (err.code != heif_error_Ok || !image) break;

        int stride = 0;
        const uint8_t* plane = heif_image_get_plane_readonly(image, heif_channel_interleaved, &stride);
        int w = heif_image_get_width(image, heif_channel_interleaved);
        int h = heif_image_get_height(image, heif_channel_interleaved);
        if (!plane || w <= 0 || h <= 0 || stride < w * 4) break;

        img.width = static_cast<UINT>(w);
        img.height = static_cast<UINT>(h);
        img.bgra.resize(static_cast<size_t>(w) * h * 4);

        for (int y = 0; y < h; ++y) {
            const uint8_t* src = plane + static_cast<size_t>(y) * stride;
            BYTE* dst = img.bgra.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x, src += 4, dst += 4) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = hasAlpha ? src[3] : 0xFF;
            }
        }
        hr = S_OK;
    } while (false);

    if (image) heif_image_release(image);
    if (use && use != primary) heif_image_handle_release(use);
    if (primary) heif_image_handle_release(primary);
    heif_context_free(ctx);
    return hr;
}

HRESULT ScaleBGRA(const DecodedImage& src, UINT newWidth, UINT newHeight, std::vector<BYTE>& out) {
    IWICImagingFactory* factory = nullptr;
    IWICBitmap* bitmap = nullptr;
    IWICBitmapScaler* scaler = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  __uuidof(IWICImagingFactory), reinterpret_cast<void**>(&factory));
    if (SUCCEEDED(hr)) {
        hr = factory->CreateBitmapFromMemory(src.width, src.height, GUID_WICPixelFormat32bppBGRA, src.width * 4,
                                             static_cast<UINT>(src.bgra.size()),
                                             const_cast<BYTE*>(src.bgra.data()), &bitmap);
    }
    if (SUCCEEDED(hr)) hr = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr)) hr = scaler->Initialize(bitmap, newWidth, newHeight, WICBitmapInterpolationModeFant);
    if (SUCCEEDED(hr)) {
        out.resize(static_cast<size_t>(newWidth) * newHeight * 4);
        hr = scaler->CopyPixels(nullptr, newWidth * 4, static_cast<UINT>(out.size()), out.data());
    }

    if (scaler) scaler->Release();
    if (bitmap) bitmap->Release();
    if (factory) factory->Release();
    return hr;
}
