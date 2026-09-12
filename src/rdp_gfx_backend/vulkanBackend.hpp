#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>
#include "gfxBackend.hpp"

namespace RDP {
class VulkanBackend : public GfxBackend {
  public:
    struct RenderTarget {
        VkImage        m_image     = VK_NULL_HANDLE;
        VkDeviceMemory m_mem       = VK_NULL_HANDLE;
        VkImageView    m_imageView = VK_NULL_HANDLE;
    };

    struct RenderOutput {
        VkImage     m_image     = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkExtent2D  m_extent    = VkExtent2D{.width = 0, .height = 0};
    };

    // Vulkan push constants
    struct RdpRenderPassConstants {
        uint32_t primColour; // RGBA
    };

    struct CurrentRenderPass {
        bool                   active        = false;
        std::vector<int32_t>   vertexData    = {};
        uint32_t               primColour    = 0;
        RdpRenderPassConstants pushConstants = {};
        auto                   reset() -> void {
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

    auto addTriangle(const int32_t* vtxs) -> void override;
    auto setPrimitiveColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> void override;
    auto startRenderPass() -> void override;
    auto completeRenderFrame() -> void override;
    auto getRenderOutput() -> RenderOutput;

    auto queueMutex() -> std::mutex& {
        return m_queueMutex;
    }

    auto dumpToFile() -> void;

  private:
    constexpr static auto MAX_BUFFERS = 2uz;

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

    VkBuffer       m_vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMem    = VK_NULL_HANDLE;
    void*          m_vertexBufferMapped = nullptr;
    std::size_t    m_vertexBufferSize   = 0;

    std::array<RenderTarget, 2> m_renderTargets;
    VkRenderPass                m_renderPass     = VK_NULL_HANDLE;
    VkPipeline                  m_pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout            m_pipelineLayout = VK_NULL_HANDLE;

    bool                     m_renderedAtLeastOnce    = false;
    bool                     m_initialized            = false;
    bool                     m_renderTargetHasContent = false;
    std::size_t              m_renderTargetWriteIndex = 0;
    std::atomic<std::size_t> m_renderTargetReadIndex  = 0;

    CurrentRenderPass m_currentRenderPass;
};

}; // namespace RDP