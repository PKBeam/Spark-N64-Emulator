#pragma once

#include <cstdint>
#include <cstddef>

namespace RDP {
enum class TextureFormat {
    INVALID,
    RGBA16,
    RGBA32,
    YUV16,
    CI4,
    CI8,
    IA4,
    IA8,
    IA16,
    I4,
    I8,
};

struct SamplerParams {
    int8_t   shift;
    uint8_t  mirror;
    uint8_t  clamp;
    uint8_t  padding_ = 0;
    uint32_t mask;
};
static_assert(sizeof(SamplerParams) == 8);

struct TileParams {
    uint32_t      width;
    uint32_t      height;
    uint32_t      stride;
    TextureFormat format;
    SamplerParams s;
    SamplerParams t;
};

struct CombineInputs {
    struct Components {
        int32_t a;
        int32_t b;
        int32_t c;
        int32_t d;
    };
    struct UsePrevious { // semantically boolean; u32 used for Vulkan shaders
        uint32_t a;
        uint32_t b;
        uint32_t c;
        uint32_t d;
    };
    Components  rgba0;
    Components  rgba1;
    UsePrevious usePreviousRgb;
    UsePrevious usePreviousAlpha;
};

class GfxBackend {
  public:
    virtual auto completeRenderFrame() -> void = 0;
    // (re)builds the sampleable GPU texture for one tile by decoding directly from the
    // caller's TMEM (tmemBytes must point at the full, currently-live TMEM contents);
    // paletteAddress is only used for the CI4/CI8 formats
    virtual auto updateTile(std::size_t      index,
                            const std::byte* data,
                            TileParams       params) -> void = 0;
    // these apply per-render pass
    virtual auto addTriangle(uint32_t         tile,
                             const std::byte* vtxBytes,
                             const std::byte* shadeBytes,
                             const std::byte* uvBytes) -> void         = 0;
    virtual auto setCombineInputs(const CombineInputs& inputs) -> void = 0;
    virtual auto startRenderPass() -> void                             = 0;

    virtual ~GfxBackend() = default;
};

auto createGfxBackend() -> GfxBackend*;
} // namespace RDP
