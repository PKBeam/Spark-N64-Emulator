#pragma once

#include <cstdint>

namespace RDP {
class GfxBackend {
  public:
    virtual auto completeRenderFrame() -> void = 0;
    // these apply per-render pass
    virtual auto addTriangle(const int32_t* vtxs) -> void                               = 0;
    virtual auto setPrimitiveColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> void = 0;
    virtual auto startRenderPass() -> void                                              = 0;

    virtual ~GfxBackend() = default;
};

auto createGfxBackend() -> GfxBackend*;
} // namespace RDP
