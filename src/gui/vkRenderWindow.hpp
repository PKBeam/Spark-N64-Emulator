#pragma once

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QTimer>
#include <QVulkanWindow>
#include <QVulkanFunctions>
#include <QFile>

#include "vkUtil.hpp"

#include <rdp_gfx_backend/vulkanBackend.hpp>

namespace GUI {

class VulkanRenderer : public QVulkanWindowRenderer {
  public:
    VulkanRenderer(QVulkanWindow* w, RDP::VulkanBackend* rdpBackend) : m_window(w), m_rdpBackend(rdpBackend) {}

    // QVulkanWindowRenderer
    auto initResources() -> void override;
    auto initSwapChainResources() -> void override;
    auto releaseSwapChainResources() -> void override;
    auto releaseResources() -> void override;
    auto startNextFrame() -> void override;

  private:
    auto renderNothing(VkImage swapImage, VkImageView swapView) -> void;

    QVulkanWindow*      m_window;
    RDP::VulkanBackend* m_rdpBackend;

    VkSampler                    m_sampler        = VK_NULL_HANDLE;
    VkDescriptorPool             m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    VkDescriptorSetLayout        m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout             m_pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline                   m_pipeline            = VK_NULL_HANDLE;
};
}; // namespace GUI