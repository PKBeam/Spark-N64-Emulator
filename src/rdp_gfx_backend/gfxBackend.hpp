#pragma once

#include <cstdint>
#include <cstddef>

namespace RDP {
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
    // these apply per-render pass
    virtual auto addTriangle(const std::byte* vtxBytes) -> void        = 0;
    virtual auto setCombineInputs(const CombineInputs& inputs) -> void = 0;
    virtual auto startRenderPass() -> void                             = 0;

    virtual ~GfxBackend() = default;
};

auto createGfxBackend() -> GfxBackend*;
} // namespace RDP
