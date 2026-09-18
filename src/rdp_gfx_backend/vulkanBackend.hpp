#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>
#include "gfxBackend.hpp"

namespace RDP {
struct VulkanTextureFormat {
    VkFormat           vkFormat;
    VkComponentMapping swizzle;
    std::size_t        bytesPerTexel;
};
class VulkanBackend : public GfxBackend {
  public:
    struct RenderTarget {
        VkImage        m_image          = VK_NULL_HANDLE;
        VkDeviceMemory m_mem            = VK_NULL_HANDLE;
        VkImageView    m_imageView      = VK_NULL_HANDLE;
        VkImage        m_depthImage     = VK_NULL_HANDLE;
        VkDeviceMemory m_depthMem       = VK_NULL_HANDLE;
        VkImageView    m_depthImageView = VK_NULL_HANDLE;
    };

    struct RenderOutput {
        VkImage     m_image     = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkExtent2D  m_extent    = VkExtent2D{.width = 0, .height = 0};
    };

    // Vulkan push constants
    struct RdpRenderPassConstants {
        RDP::CombineInputs combineInputs;
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
        uint32_t                      primColour    = 0;
        RdpRenderPassConstants        pushConstants = {};
        std::array<ShaderTileInfo, 8> tileParams    = {};

        auto reset() -> void {
            active = false;
            vertexData.clear();
            pushConstants = {};
        }
    };

    VulkanBackend()  = default;
    ~VulkanBackend() = default;

    VulkanBackend(const VulkanBackend&)            = delete ("Only one RDP render instance allowed");
    VulkanBackend& operator=(const VulkanBackend&) = delete ("Only one RDP render instance allowed");

    auto init(
        VkDevice         vkDevice,
        VkInstance       vkInstance,
        VkPhysicalDevice vkPhysicalDevice,
        uint32_t         vkQueueFamilyIndex) -> void;

    auto destroy() -> void;

    auto findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t;
    auto reallocVertexBuffer(std::size_t newSize) -> void;

    auto createRenderTargets() -> void;
    auto createCmdObjects() -> void;
    auto createPipeline() -> void;
    auto createDescriptorSetLayout() -> void;
    auto createDescriptorPool() -> void;
    auto createFallbackTexture() -> void;
    auto createDescriptorSet() -> void;
    auto createTileParamsBuffer() -> void;

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
    auto startRenderPass(RenderOptions options) -> void override;
    auto completeRenderFrame() -> void override;
    auto getRenderOutput() -> RenderOutput;

    using FrameCompleteCallback = void (*)(void*);
    auto setFrameCompleteCallback(FrameCompleteCallback callback, void* userData) -> void;

    auto queueMutex() -> std::mutex& {
        return m_queueMutex;
    }

    auto dumpToFile() -> void;

  private:
    constexpr static auto MAX_BUFFERS = 2uz;
    constexpr static auto NUM_TILES   = 8uz;
    constexpr static auto TMEM_SIZE   = 4096uz; // RDP TMEM is a fixed 4KB

    VkDevice         m_vkDevice           = VK_NULL_HANDLE;
    VkInstance       m_vkInstance         = VK_NULL_HANDLE;
    VkPhysicalDevice m_vkPhysicalDevice   = VK_NULL_HANDLE;
    uint32_t         m_vkQueueFamilyIndex = 0;
    VkQueue          m_vkQueue            = VK_NULL_HANDLE;

    VkCommandPool   m_commandPool   = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkFence         m_renderFence   = VK_NULL_HANDLE;
    std::mutex      m_resourceMutex;
    std::mutex      m_queueMutex; // todo find a better home for this
    std::mutex      m_callbackMutex;

    FrameCompleteCallback m_frameCompleteCallback = nullptr;
    void*                 m_frameCompleteUserData  = nullptr;

    auto notifyFrameComplete() -> void;

    VkBuffer       m_vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMem    = VK_NULL_HANDLE;
    void*          m_vertexBufferMapped = nullptr;
    std::size_t    m_vertexBufferSize   = 0;

    VkBuffer       m_tileParamsBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_tileParamsBufferMem    = VK_NULL_HANDLE;
    void*          m_tileParamsBufferMapped = nullptr;

    std::array<RenderTarget, 2> m_renderTargets;
    VkRenderPass                m_renderPass     = VK_NULL_HANDLE;
    VkPipeline                  m_pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout            m_pipelineLayout = VK_NULL_HANDLE;

    struct VulkanTile {
        VkImage        image     = VK_NULL_HANDLE;
        VkDeviceMemory imageMem  = VK_NULL_HANDLE;
        VkImageView    imageView = VK_NULL_HANDLE;
        VkSampler      sampler   = VK_NULL_HANDLE;
    };
    // the currently bound tile's texture, resolved to a native VkFormat (CPU-decoded to
    // RGBA8 first for palette/sub-byte-packed formats so it can still be hardware-sampled)
    std::array<VulkanTile, NUM_TILES> m_textures;
    VulkanTile                        m_fallbackTexture;
    std::vector<std::byte>            m_textureMemory;

    VkDescriptorSetLayout m_textureDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_textureDescriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_textureDescriptorSet       = VK_NULL_HANDLE;

    VkExtent2D m_extent = {320, 240};

    bool                     m_renderedAtLeastOnce    = false;
    bool                     m_initialized            = false;
    bool                     m_renderTargetHasContent = false;
    std::size_t              m_renderTargetWriteIndex = 0;
    std::atomic<std::size_t> m_renderTargetReadIndex  = 0;

    CurrentRenderPass m_currentRenderPass;
};

}; // namespace RDP