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
    uint32_t offset;
};
static_assert(sizeof(SamplerParams) == 12);

constexpr auto NO_TILE = static_cast<uint32_t>(-1);

struct TileParams {
    uint32_t      width;
    uint32_t      height;
    uint32_t      stride;
    TextureFormat format;
    SamplerParams s;
    SamplerParams t;
    uint8_t       subPalette;
};

struct CombineInputs {
    enum class Source : uint8_t {
        ZERO = 0,
        ONE,
        NOISE,

        COMBINED,
        COMBINED_ALPHA,

        TEX0,
        TEX0_ALPHA,

        TEX1,
        TEX1_ALPHA,

        PRIMITIVE,
        PRIMITIVE_ALPHA,

        SHADE,
        SHADE_ALPHA,

        ENVIRONMENT,
        ENVIRONMENT_ALPHA,

        LOD_FRACTION,
        PRIM_LOD_FRAC,

        CENTER,
        SCALE,
        K4,
        K5,
    };
    static_assert(static_cast<uint8_t>(Source::K5) == 20);

    struct Uniform {
        uint32_t primitive;
        uint32_t environment;
        uint32_t lodFraction;
        uint32_t primLodFrac;
        uint32_t scale;
        uint32_t center;
        uint32_t k4;
        uint32_t k5;
    };
    struct Select {
        Source a;
        Source b;
        Source c;
        Source d;
    };
    Uniform uniform;
    Select  rgb[2];
    Select  alpha[2];
};

struct RenderOptions {
    bool depthTestEnable  = true;
    bool depthWriteEnable = true;
};

class GfxBackend {
  public:
    virtual auto completeRenderFrame() -> void = 0;
    // (re)builds the sampleable GPU texture for one tile by decoding directly from the
    // caller's TMEM (tmemBytes must point at the full, currently-live TMEM contents);
    // paletteAddress is only used for the CI4/CI8 formats
    virtual auto updateTile(std::size_t      index,
                            TileParams       params,
                            const std::byte* texelData,
                            TextureFormat    paletteFormat,
                            const std::byte* paletteData = nullptr) -> void = 0;
    // these apply per-render pass
    virtual auto addTriangle(uint32_t         tile,
                             const std::byte* vtxBytes,
                             const std::byte* shadeBytes,
                             const std::byte* uvBytes) -> void         = 0;
    virtual auto setCombineInputs(const CombineInputs& inputs) -> void = 0;
    virtual auto setBlendColour(uint32_t blendColour) -> void          = 0;
    virtual auto setFillColour(uint32_t fillColour) -> void            = 0;
    virtual auto setZModeDecal(bool enable) -> void                    = 0;
    virtual auto startRenderPass(RenderOptions options) -> void        = 0;

    virtual ~GfxBackend() = default;
};

auto createGfxBackend() -> GfxBackend*;
} // namespace RDP
