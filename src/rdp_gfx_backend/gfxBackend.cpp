#include "gfxBackend.hpp"
#include "vulkanBackend.hpp"

namespace RDP {
auto createGfxBackend() -> GfxBackend* {
    return new VulkanBackend();
}
} // namespace RDP