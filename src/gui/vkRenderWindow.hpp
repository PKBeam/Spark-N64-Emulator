#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

#include <processor/rdp/gfx/vulkanBackend.hpp>

class QVulkanInstance;
class QWindow;

namespace GUI {

class VulkanRenderer {
  public:
    VulkanRenderer(QWindow* window, QVulkanInstance* instance, RDP::VulkanBackend* rdpBackend);
    ~VulkanRenderer();

    auto render() -> void;
    auto shutdown() -> void;
    auto markSwapChainDirty() -> void;

  private:
    static constexpr auto FramesInFlight = 2u;

    auto initialize() -> void;
    auto createDevice() -> void;
    auto createDeviceResources() -> void;
    auto recreateSwapChain() -> bool;
    auto destroySwapChainResources() -> void;
    auto createPipeline() -> void;
    auto recordFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex) -> void;

    QWindow*            m_window;
    QVulkanInstance*    m_instance;
    RDP::VulkanBackend* m_rdpBackend;

    VkInstance       m_vkInstance           = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface              = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice       = VK_NULL_HANDLE;
    VkDevice         m_device               = VK_NULL_HANDLE;
    uint32_t         m_graphicsQueueFamily  = 0;
    uint32_t         m_presentQueueFamily   = 0;
    VkQueue          m_graphicsQueue        = VK_NULL_HANDLE;
    VkQueue          m_presentQueue         = VK_NULL_HANDLE;
    VkCommandPool    m_commandPool          = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, FramesInFlight> m_commandBuffers{};
    std::array<VkSemaphore, FramesInFlight>     m_imageAvailable{};
    std::array<VkFence, FramesInFlight>         m_inFlightFences{};

    VkSwapchainKHR             m_swapChain = VK_NULL_HANDLE;
    VkFormat                   m_colorFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D                 m_swapChainExtent{};
    std::vector<VkImage>       m_swapChainImages;
    std::vector<VkImageView>   m_swapChainImageViews;
    std::vector<VkSemaphore>   m_renderFinished;
    std::vector<VkFence>       m_imagesInFlight;
    VkSampler                  m_sampler = VK_NULL_HANDLE;
    VkDescriptorPool           m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    VkDescriptorSetLayout      m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout           m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline                 m_pipeline = VK_NULL_HANDLE;

    uint32_t m_currentFrame = 0;
    bool     m_initialized = false;
    bool     m_backendInitialized = false;
    bool     m_swapChainDirty = true;
};

} // namespace GUI
