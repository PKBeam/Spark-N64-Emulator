#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <flat_map>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>
#include <util/vkUtil.hpp>
#include "gfxBackend.hpp"

namespace RDP {
struct VulkanTextureFormat {
    VkFormat           vkFormat;
    VkComponentMapping swizzle;
    std::size_t        bytesPerTexel;
};
struct RenderTarget {
    Util::VK::AllocatedImage colour;
    Util::VK::AllocatedImage depth;
};

struct RenderOutput {
    VkImage     m_image     = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkExtent2D  m_extent    = VkExtent2D{.width = 0, .height = 0};
};

// Vulkan push constants
struct RdpRenderPassConstants {
    RDP::CombineInputs combineInputs;
    uint32_t           blendColour;
};

struct ShaderTileInfo {
    std::array<uint32_t, 4> extent;
    RDP::SamplerParams      s;
    RDP::SamplerParams      t;
};
static_assert(sizeof(ShaderTileInfo) == 40);

struct CurrentRenderPass {
    bool                          active        = false;
    std::vector<int32_t>          vertexData    = {};
    RdpRenderPassConstants        pushConstants = {};
    std::array<ShaderTileInfo, 8> tileParams    = {};

    auto reset() -> void {
        active = false;
        vertexData.clear();
        pushConstants = {};
    }
};

struct TextureCacheKey {
    struct Info {
        uint8_t width;
        uint8_t height;
    };
    struct Data {
        uint64_t       textureHash                          = 0;
        uint64_t       paletteHash                          = 0;
        constexpr auto operator<=>(const Data& other) const = default;
    };
    Info           info = {};
    Data           data = {};
    constexpr auto operator<=>(const TextureCacheKey& other) const {
        return data <=> other.data;
    };
};

class VulkanBackend : public GfxBackend {
  public:
    VulkanBackend()  = default;
    ~VulkanBackend() = default;

    VulkanBackend(const VulkanBackend&)            = delete ("Only one RDP render instance allowed");
    VulkanBackend& operator=(const VulkanBackend&) = delete ("Only one RDP render instance allowed");

    // GfxBackend impl
    auto updateTile(std::size_t      index,
                    TileParams       params,
                    const std::byte* texelData,
                    TextureFormat    paletteFormat,
                    const std::byte* paletteData) -> void override;
    auto addTriangle(uint32_t         tile,
                     const std::byte* vtxBytes,
                     const std::byte* shadeBytes = nullptr,
                     const std::byte* uvBytes    = nullptr) -> void override;
    auto setCombineInputs(const CombineInputs& inputs) -> void override;
    auto setBlendColour(uint32_t blendColour) -> void override;
    auto setZModeDecal(bool enable) -> void override;
    auto startRenderPass(RenderOptions options) -> void override;
    auto completeRenderFrame() -> void override;
    auto getRenderOutput() -> RenderOutput;

    using FrameCompleteCallback = void (*)(void*);
    auto setFrameCompleteCallback(FrameCompleteCallback callback, void* userData) -> void;

    // setup and teardown
    auto init(
        VkDevice         vkDevice,
        VkInstance       vkInstance,
        VkPhysicalDevice vkPhysicalDevice,
        uint32_t         vkQueueFamilyIndex) -> void;
    auto createRenderTargets() -> void;
    auto createDescriptorInfo() -> void;
    auto createPipeline() -> void;
    auto createCommandBuffer() -> void;
    auto createTextures() -> void;
    auto createDescriptorSets() -> void;
    auto destroy() -> void;

    auto queueMutex() -> std::mutex& {
        return m_queueMutex;
    }

  private:
    auto createDefaultTexture() -> void;
    auto reallocVertexBuffer(std::size_t newSize) -> void;

    constexpr static auto MAX_BUFFERS = 2uz;
    constexpr static auto NUM_TILES   = 8uz;
    constexpr static auto TMEM_SIZE   = 4096uz; // RDP TMEM is a fixed 4KB

    VkDevice         m_vkDevice = VK_NULL_HANDLE;
    Util::VK::Device m_device;
    VkPhysicalDevice m_vkPhysicalDevice   = VK_NULL_HANDLE;
    VkInstance       m_vkInstance         = VK_NULL_HANDLE;
    uint32_t         m_vkQueueFamilyIndex = 0;
    VkQueue          m_vkQueue            = VK_NULL_HANDLE;

    VkCommandPool   m_commandPool   = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkFence         m_renderFence   = VK_NULL_HANDLE;
    std::mutex      m_resourceMutex;
    std::mutex      m_queueMutex; // todo find a better home for this
    std::mutex      m_callbackMutex;

    FrameCompleteCallback m_frameCompleteCallback = nullptr;
    void*                 m_frameCompleteUserData = nullptr;

    Util::VK::AllocatedBuffer m_vertexBuffer{};
    Util::VK::AllocatedBuffer m_tileStagingBuffer{};

    Util::VK::AllocatedBuffer m_tileParamsBuffer{};

    std::array<RenderTarget, 2> m_renderTargets;
    VkRenderPass                m_renderPass     = VK_NULL_HANDLE;
    VkPipeline                  m_pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout            m_pipelineLayout = VK_NULL_HANDLE;

    // the currently bound tile's texture, resolved to a native VkFormat (CPU-decoded to
    // RGBA8 first for palette/sub-byte-packed formats so it can still be hardware-sampled)
    std::flat_map<TextureCacheKey, Util::VK::AllocatedTexture> m_textureCache;
    Util::VK::AllocatedTexture                                 m_fallbackTexture;
    std::vector<std::byte>                                     m_textureMemory;
    Util::VK::TextureDescriptors<NUM_TILES>                    m_textureDescriptors{0};
    Util::VK::BufferDescriptors<1>                             m_tileParamsDescriptors{NUM_TILES};

    VkDescriptorSetLayout m_textureDescriptorSetLayout = VK_NULL_HANDLE;

    VkExtent2D m_extent = {320, 240};

    bool                     m_zModeDecal             = false;
    bool                     m_renderedAtLeastOnce    = false;
    bool                     m_initialized            = false;
    bool                     m_renderTargetHasContent = false;
    std::size_t              m_renderTargetWriteIndex = 0;
    std::atomic<std::size_t> m_renderTargetReadIndex  = 0;

    CurrentRenderPass m_currentRenderPass;
};

}; // namespace RDP