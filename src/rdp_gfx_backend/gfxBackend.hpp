#pragma once

#include <cstdint>

namespace RDP {
class GfxBackend {
  public:
    virtual auto addTriangle(const int32_t* vtxs) -> void = 0;
    virtual auto renderFrame() -> void                    = 0;

    virtual ~GfxBackend() = default;
};

auto createGfxBackend() -> GfxBackend*;
} // namespace RDP
