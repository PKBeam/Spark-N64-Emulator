#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

#include <QVulkanInstance>
#include <QLoggingCategory>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

#include <util/vkUtil.hpp>
#include "vkRenderWindow.hpp"

// clang-format off
static const auto WindowVertexShaderSpv = std::vector<uint8_t> {
#embed <window.vert.spv>
};

static const auto WindowFragmentShaderSpv = std::vector<uint8_t> {
#embed <window.frag.spv>
};
// clang-format on

namespace GUI {
namespace {

auto checkVk(VkResult result, std::string_view operation) -> void {
    if (result != VK_SUCCESS) {
        throw Util::VK::Error("{} failed with VkResult({})", operation, static_cast<int>(result));
    }
}

auto require(bool condition, std::string_view message) -> void {
    if (!condition) {
        throw Util::VK::Error("{}", message);
    }
}

auto hasDeviceExtension(VkPhysicalDevice device, const char* extensionName) -> bool {
    uint32_t extensionCount = 0;
    checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
    auto extensions = std::vector<VkExtensionProperties>(extensionCount);
    checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()), "vkEnumerateDeviceExtensionProperties");
    return std::ranges::any_of(extensions, [extensionName](const auto& extension) {
        return std::strcmp(extension.extensionName, extensionName) == 0;
    });
}

struct QueueFamilies {
    uint32_t graphics = std::numeric_limits<uint32_t>::max();
    uint32_t present  = std::numeric_limits<uint32_t>::max();

    auto valid() const -> bool {
        return graphics != std::numeric_limits<uint32_t>::max() && present != std::numeric_limits<uint32_t>::max();
    }
};

auto findQueueFamilies(VkPhysicalDevice device, QVulkanInstance* instance, QWindow* window) -> QueueFamilies {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    auto properties = std::vector<VkQueueFamilyProperties>(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

    auto families = QueueFamilies{};
    for (const auto i : std::views::iota(0u, count)) {
        if (properties[i].queueCount == 0) {
            continue;
        }
        if ((properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && families.graphics == std::numeric_limits<uint32_t>::max()) {
            families.graphics = i;
        }
        if (instance->supportsPresent(device, i, window) && families.present == std::numeric_limits<uint32_t>::max()) {
            families.present = i;
        }
        if (families.graphics == i && families.present == i) {
            break;
        }
    }
    return families;
}

auto supportsRequiredFeatures(VkPhysicalDevice device, bool portabilitySubset) -> bool {
    auto supported12           = VkPhysicalDeviceVulkan12Features{};
    supported12.sType          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    auto supported13           = VkPhysicalDeviceVulkan13Features{};
    supported13.sType          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    auto supported14           = VkPhysicalDeviceVulkan14Features{};
    supported14.sType          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    auto supportedPortability  = VkPhysicalDevicePortabilitySubsetFeaturesKHR{};
    supportedPortability.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
    supported12.pNext          = &supported13;
    supported13.pNext          = &supported14;
    supported14.pNext          = portabilitySubset ? &supportedPortability : nullptr;

    auto supported  = VkPhysicalDeviceFeatures2{};
    supported.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported.pNext = &supported12;
    vkGetPhysicalDeviceFeatures2(device, &supported);

    return supported12.shaderInt8 && supported12.uniformAndStorageBuffer8BitAccess && supported12.storagePushConstant8 &&
           supported12.scalarBlockLayout && supported12.descriptorBindingSampledImageUpdateAfterBind &&
           supported12.descriptorBindingUniformBufferUpdateAfterBind && supported13.dynamicRendering &&
           supported13.synchronization2 && supported13.shaderDemoteToHelperInvocation && supported14.pushDescriptor &&
           (!portabilitySubset || supportedPortability.imageViewFormatSwizzle);
}

auto preferredSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) -> VkSurfaceFormatKHR {
    require(!formats.empty(), "The Vulkan surface has no supported formats");
    if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
        return VkSurfaceFormatKHR{
            .format     = VK_FORMAT_B8G8R8A8_SRGB,
            .colorSpace = formats.front().colorSpace,
        };
    }
    for (const auto format : {
             VK_FORMAT_B8G8R8A8_SRGB,
             VK_FORMAT_R8G8B8A8_SRGB,
             VK_FORMAT_B8G8R8A8_UNORM,
             VK_FORMAT_R8G8B8A8_UNORM,
         }) {
        const auto found = std::ranges::find_if(formats, [format](const auto& candidate) {
            return candidate.format == format && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
        if (found != formats.end()) {
            return *found;
        }
    }
    return formats.front();
}

auto preferredCompositeAlpha(VkCompositeAlphaFlagsKHR supported) -> VkCompositeAlphaFlagBitsKHR {
    for (const auto mode : {
             VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
             VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
             VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
             VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
         }) {
        if ((supported & mode) != 0) {
            return mode;
        }
    }
    throw Util::VK::Error("The Vulkan surface has no supported composite alpha mode");
}

} // namespace

VulkanRenderer::VulkanRenderer(QWindow* window, QVulkanInstance* instance, RDP::VulkanBackend* rdpBackend)
    : m_window(window), m_instance(instance), m_rdpBackend(rdpBackend) {}

VulkanRenderer::~VulkanRenderer() {
    try {
        shutdown();
    } catch (const std::exception& error) {
        qWarning("Vulkan renderer shutdown failed: %s", error.what());
    }
}

auto VulkanRenderer::initialize() -> void {
    if (m_initialized) {
        return;
    }
    require(m_window != nullptr && m_instance != nullptr && m_rdpBackend != nullptr, "Vulkan renderer is missing initialization dependencies");
    m_vkInstance = m_instance->vkInstance();
    require(m_vkInstance != VK_NULL_HANDLE, "The Qt Vulkan instance is not initialized");
    m_surface = QVulkanInstance::surfaceForWindow(m_window);
    require(m_surface != VK_NULL_HANDLE, "Qt could not create a Vulkan surface for the window");

    createDevice();
    createDeviceResources();
    m_rdpBackend->init(m_device, m_vkInstance, m_physicalDevice, m_graphicsQueueFamily);
    m_backendInitialized = true;
    m_initialized        = true;
}

auto VulkanRenderer::createDevice() -> void {
    uint32_t physicalDeviceCount = 0;
    checkVk(vkEnumeratePhysicalDevices(m_vkInstance, &physicalDeviceCount, nullptr), "vkEnumeratePhysicalDevices");
    require(physicalDeviceCount > 0, "No Vulkan physical devices were found");
    auto physicalDevices = std::vector<VkPhysicalDevice>(physicalDeviceCount);
    checkVk(vkEnumeratePhysicalDevices(m_vkInstance, &physicalDeviceCount, physicalDevices.data()), "vkEnumeratePhysicalDevices");

    for (const auto physicalDevice : physicalDevices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_4) {
            qWarning("Vulkan device '%s' rejected: Vulkan 1.4 is required", properties.deviceName);
            continue;
        }
        if (!hasDeviceExtension(physicalDevice, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            qWarning("Vulkan device '%s' rejected: VK_KHR_swapchain is not supported", properties.deviceName);
            continue;
        }
        const auto portabilitySubset = hasDeviceExtension(physicalDevice, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        if (!supportsRequiredFeatures(physicalDevice, portabilitySubset)) {
            qWarning("Vulkan device '%s' rejected: required Vulkan features are not supported", properties.deviceName);
            continue;
        }
        const auto queueFamilies = findQueueFamilies(physicalDevice, m_instance, m_window);
        if (!queueFamilies.valid()) {
            qWarning("Vulkan device '%s' rejected: graphics or presentation queue unavailable", properties.deviceName);
            continue;
        }
        m_physicalDevice      = physicalDevice;
        m_graphicsQueueFamily = queueFamilies.graphics;
        m_presentQueueFamily  = queueFamilies.present;
        break;
    }
    require(m_physicalDevice != VK_NULL_HANDLE, "No Vulkan device supports the required features and window presentation");

    auto           queueFamilies    = std::array<uint32_t, 2>{m_graphicsQueueFamily, m_presentQueueFamily};
    const auto     queueCount       = m_graphicsQueueFamily == m_presentQueueFamily ? 1u : 2u;
    constexpr auto queuePriority    = 1.0f;
    auto           queueCreateInfos = std::array<VkDeviceQueueCreateInfo, 2>{};
    for (const auto i : std::views::iota(0u, queueCount)) {
        queueCreateInfos[i] = VkDeviceQueueCreateInfo{
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = queueFamilies[i],
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };
    }

    const auto portabilitySubset                             = hasDeviceExtension(m_physicalDevice, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    auto       features12                                    = VkPhysicalDeviceVulkan12Features{};
    features12.sType                                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.shaderInt8                                    = VK_TRUE;
    features12.uniformAndStorageBuffer8BitAccess             = VK_TRUE;
    features12.storagePushConstant8                          = VK_TRUE;
    features12.scalarBlockLayout                             = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE;
    features12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    auto features13                                          = VkPhysicalDeviceVulkan13Features{};
    features13.sType                                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering                              = VK_TRUE;
    features13.synchronization2                              = VK_TRUE;
    features13.shaderDemoteToHelperInvocation                = VK_TRUE;
    auto features14                                          = VkPhysicalDeviceVulkan14Features{};
    features14.sType                                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    features14.pushDescriptor                                = VK_TRUE;
    auto portabilityFeatures                                 = VkPhysicalDevicePortabilitySubsetFeaturesKHR{};
    portabilityFeatures.sType                                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
    portabilityFeatures.imageViewFormatSwizzle               = VK_TRUE;
    features12.pNext                                         = &features13;
    features13.pNext                                         = &features14;
    features14.pNext                                         = portabilitySubset ? &portabilityFeatures : nullptr;
    auto enabledFeatures                                     = VkPhysicalDeviceFeatures2{};
    enabledFeatures.sType                                    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enabledFeatures.pNext                                    = &features12;
    auto extensions                                          = std::vector<const char*>{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    if (portabilitySubset) {
        extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }

    const auto deviceCreateInfo = VkDeviceCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &enabledFeatures,
        .flags                   = 0,
        .queueCreateInfoCount    = queueCount,
        .pQueueCreateInfos       = queueCreateInfos.data(),
        .enabledLayerCount       = 0,
        .ppEnabledLayerNames     = nullptr,
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
        .pEnabledFeatures        = nullptr,
    };
    checkVk(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device), "vkCreateDevice");
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
}

auto VulkanRenderer::createDeviceResources() -> void {
    const auto samplerInfo = VkSamplerCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .magFilter               = VK_FILTER_NEAREST,
        .minFilter               = VK_FILTER_NEAREST,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = VK_LOD_CLAMP_NONE,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    checkVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler), "vkCreateSampler");

    const auto descriptorSetLayoutBinding = VkDescriptorSetLayoutBinding{
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = &m_sampler,
    };
    const auto descriptorSetLayoutInfo = VkDescriptorSetLayoutCreateInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = 1,
        .pBindings    = &descriptorSetLayoutBinding,
    };
    checkVk(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutInfo, nullptr, &m_descriptorSetLayout), "vkCreateDescriptorSetLayout");

    const auto layoutInfo = VkPipelineLayoutCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1,
        .pSetLayouts            = &m_descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = nullptr,
    };
    checkVk(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout), "vkCreatePipelineLayout");

    const auto commandPoolInfo = VkCommandPoolCreateInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_graphicsQueueFamily,
    };
    checkVk(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_commandPool), "vkCreateCommandPool");
    const auto commandBufferInfo = VkCommandBufferAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = m_commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = FramesInFlight,
    };
    checkVk(vkAllocateCommandBuffers(m_device, &commandBufferInfo, m_commandBuffers.data()), "vkAllocateCommandBuffers");

    for (const auto i : std::views::iota(0u, FramesInFlight)) {
        const auto semaphoreInfo = VkSemaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };
        checkVk(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailable[i]), "vkCreateSemaphore");
        const auto fenceInfo = VkFenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        checkVk(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]), "vkCreateFence");
    }
}

auto VulkanRenderer::recreateSwapChain() -> bool {
    if (!m_window->isExposed()) {
        return false;
    }

    auto capabilities = VkSurfaceCapabilitiesKHR{};
    checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    auto extent = capabilities.currentExtent;
    if (extent.width == std::numeric_limits<uint32_t>::max()) {
        const auto ratio  = m_window->devicePixelRatio();
        const auto width  = static_cast<uint32_t>(std::max(0L, std::lround(static_cast<double>(m_window->width()) * ratio)));
        const auto height = static_cast<uint32_t>(std::max(0L, std::lround(static_cast<double>(m_window->height()) * ratio)));
        if (width == 0 || height == 0) {
            return false;
        }
        extent.width  = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0) {
        return false;
    }
    require((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0,
            "The Vulkan surface does not support color-attachment swapchain images");

    uint32_t formatCount = 0;
    checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    auto formats = std::vector<VkSurfaceFormatKHR>(formatCount);
    checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    const auto surfaceFormat = preferredSurfaceFormat(formats);

    auto imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    const auto queueFamilyIndices = std::array<uint32_t, 2>{m_graphicsQueueFamily, m_presentQueueFamily};
    const auto sharingMode        = m_graphicsQueueFamily == m_presentQueueFamily ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
    const auto swapChainInfo      = VkSwapchainCreateInfoKHR{
             .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
             .pNext                 = nullptr,
             .flags                 = 0,
             .surface               = m_surface,
             .minImageCount         = imageCount,
             .imageFormat           = surfaceFormat.format,
             .imageColorSpace       = surfaceFormat.colorSpace,
             .imageExtent           = extent,
             .imageArrayLayers      = 1,
             .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
             .imageSharingMode      = sharingMode,
             .queueFamilyIndexCount = sharingMode == VK_SHARING_MODE_CONCURRENT ? 2u : 0u,
             .pQueueFamilyIndices   = sharingMode == VK_SHARING_MODE_CONCURRENT ? queueFamilyIndices.data() : nullptr,
             .preTransform          = capabilities.currentTransform,
             .compositeAlpha        = preferredCompositeAlpha(capabilities.supportedCompositeAlpha),
             .presentMode           = VK_PRESENT_MODE_FIFO_KHR,
             .clipped               = VK_TRUE,
             .oldSwapchain          = VK_NULL_HANDLE,
    };

    {
        auto queueLock = std::lock_guard<std::mutex>(m_rdpBackend->queueMutex());
        checkVk(vkDeviceWaitIdle(m_device), "vkDeviceWaitIdle");
    }
    destroySwapChainResources();
    checkVk(vkCreateSwapchainKHR(m_device, &swapChainInfo, nullptr, &m_swapChain), "vkCreateSwapchainKHR");

    uint32_t swapChainImageCount = 0;
    checkVk(vkGetSwapchainImagesKHR(m_device, m_swapChain, &swapChainImageCount, nullptr), "vkGetSwapchainImagesKHR");
    m_swapChainImages.resize(swapChainImageCount);
    checkVk(vkGetSwapchainImagesKHR(m_device, m_swapChain, &swapChainImageCount, m_swapChainImages.data()), "vkGetSwapchainImagesKHR");
    m_swapChainExtent = extent;
    m_colorFormat     = surfaceFormat.format;
    m_swapChainImageViews.resize(m_swapChainImages.size());
    for (const auto i : std::views::iota(0uz, m_swapChainImages.size())) {
        const auto viewInfo = VkImageViewCreateInfo{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .image            = m_swapChainImages[i],
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = m_colorFormat,
            .components       = Util::VK::Defaults::ComponentMapping,
            .subresourceRange = Util::VK::Defaults::ImageSubresourceRange(),
        };
        checkVk(vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapChainImageViews[i]), "vkCreateImageView");
    }

    const auto poolSize = VkDescriptorPoolSize{
        .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = swapChainImageCount,
    };
    const auto descriptorPoolInfo = VkDescriptorPoolCreateInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .maxSets       = swapChainImageCount,
        .poolSizeCount = 1,
        .pPoolSizes    = &poolSize,
    };
    checkVk(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool), "vkCreateDescriptorPool");
    auto       layouts           = std::vector<VkDescriptorSetLayout>(swapChainImageCount, m_descriptorSetLayout);
    const auto descriptorSetInfo = VkDescriptorSetAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = nullptr,
        .descriptorPool     = m_descriptorPool,
        .descriptorSetCount = swapChainImageCount,
        .pSetLayouts        = layouts.data(),
    };
    m_descriptorSets.resize(swapChainImageCount);
    checkVk(vkAllocateDescriptorSets(m_device, &descriptorSetInfo, m_descriptorSets.data()), "vkAllocateDescriptorSets");

    m_renderFinished.resize(swapChainImageCount);
    m_imagesInFlight.assign(swapChainImageCount, VK_NULL_HANDLE);
    const auto semaphoreInfo = VkSemaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    for (auto& semaphore : m_renderFinished) {
        checkVk(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &semaphore), "vkCreateSemaphore");
    }
    createPipeline();
    m_swapChainDirty = false;
    return true;
}

auto VulkanRenderer::destroySwapChainResources() -> void {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    m_descriptorSets.clear();
    for (const auto semaphore : m_renderFinished) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, semaphore, nullptr);
        }
    }
    m_renderFinished.clear();
    m_imagesInFlight.clear();
    for (const auto view : m_swapChainImageViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, view, nullptr);
        }
    }
    m_swapChainImageViews.clear();
    m_swapChainImages.clear();
    if (m_swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }
}

auto VulkanRenderer::createPipeline() -> void {
    const auto vertInfo         = Util::VK::shaderModuleCreateInfo(WindowVertexShaderSpv);
    auto       vertShaderModule = VkShaderModule{VK_NULL_HANDLE};
    checkVk(vkCreateShaderModule(m_device, &vertInfo, nullptr, &vertShaderModule), "vkCreateShaderModule");
    const auto fragInfo         = Util::VK::shaderModuleCreateInfo(WindowFragmentShaderSpv);
    auto       fragShaderModule = VkShaderModule{VK_NULL_HANDLE};
    try {
        checkVk(vkCreateShaderModule(m_device, &fragInfo, nullptr, &fragShaderModule), "vkCreateShaderModule");

        const auto shaderStages = std::array<VkPipelineShaderStageCreateInfo, 2>{
            VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = VK_SHADER_STAGE_VERTEX_BIT,
                .module              = vertShaderModule,
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            },
            VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module              = fragShaderModule,
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            },
        };
        const auto vertexInput = VkPipelineVertexInputStateCreateInfo{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = 0,
            .pVertexBindingDescriptions      = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions    = nullptr,
        };
        const auto inputAssembly = VkPipelineInputAssemblyStateCreateInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };
        const auto dynamicStates = std::array<VkDynamicState, 2>{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const auto dynamicState  = VkPipelineDynamicStateCreateInfo{
             .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
             .pNext             = nullptr,
             .flags             = 0,
             .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
             .pDynamicStates    = dynamicStates.data(),
        };
        const auto viewportState = VkPipelineViewportStateCreateInfo{
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .viewportCount = 1,
            .pViewports    = nullptr,
            .scissorCount  = 1,
            .pScissors     = nullptr,
        };
        const auto rasterState = VkPipelineRasterizationStateCreateInfo{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_NONE,
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp          = 0.0f,
            .depthBiasSlopeFactor    = 0.0f,
            .lineWidth               = 1.0f,
        };
        const auto multisampleState = VkPipelineMultisampleStateCreateInfo{
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 1.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
        };
        const auto colorBlendAttachment = VkPipelineColorBlendAttachmentState{
            .blendEnable         = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const auto colorBlendState = VkPipelineColorBlendStateCreateInfo{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments    = &colorBlendAttachment,
            .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
        };
        const auto renderingInfo = VkPipelineRenderingCreateInfo{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext                   = nullptr,
            .viewMask                = 0,
            .colorAttachmentCount    = 1,
            .pColorAttachmentFormats = &m_colorFormat,
            .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
        };
        const auto pipelineInfo = VkGraphicsPipelineCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = &renderingInfo,
            .flags               = 0,
            .stageCount          = static_cast<uint32_t>(shaderStages.size()),
            .pStages             = shaderStages.data(),
            .pVertexInputState   = &vertexInput,
            .pInputAssemblyState = &inputAssembly,
            .pTessellationState  = nullptr,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterState,
            .pMultisampleState   = &multisampleState,
            .pDepthStencilState  = nullptr,
            .pColorBlendState    = &colorBlendState,
            .pDynamicState       = &dynamicState,
            .layout              = m_pipelineLayout,
            .renderPass          = VK_NULL_HANDLE,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        };
        checkVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "vkCreateGraphicsPipelines");
    } catch (...) {
        vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
        if (fragShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
        }
        throw;
    }
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
}

auto VulkanRenderer::recordFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex) -> void {
    const auto beginInfo = VkCommandBufferBeginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    checkVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    const auto swapImage  = m_swapChainImages[imageIndex];
    const auto preBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask       = VK_ACCESS_2_NONE,
        .dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swapImage,
        .subresourceRange    = Util::VK::Defaults::ImageSubresourceRange(),
    };
    const auto preDependency = VkDependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = 0,
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers    = nullptr,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &preBarrier,
    };
    vkCmdPipelineBarrier2(commandBuffer, &preDependency);

    const auto colorAttachment = VkRenderingAttachmentInfo{
        .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext              = nullptr,
        .imageView          = m_swapChainImageViews[imageIndex],
        .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode        = VK_RESOLVE_MODE_NONE,
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue         = VkClearValue{.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    const auto renderingInfo = VkRenderingInfo{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext                = nullptr,
        .flags                = 0,
        .renderArea           = VkRect2D{.offset = {0, 0}, .extent = m_swapChainExtent},
        .layerCount           = 1,
        .viewMask             = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachment,
        .pDepthAttachment     = nullptr,
        .pStencilAttachment   = nullptr,
    };
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    const auto frame = m_rdpBackend->getRenderOutput();
    if (frame.m_image != VK_NULL_HANDLE && frame.m_extent.width > 0 && frame.m_extent.height > 0) {
        const auto imageInfo = VkDescriptorImageInfo{
            .sampler     = m_sampler,
            .imageView   = frame.m_imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const auto writeDescriptor = VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = m_descriptorSets[imageIndex],
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &imageInfo,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        };
        vkUpdateDescriptorSets(m_device, 1, &writeDescriptor, 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[imageIndex], 0, nullptr);
        const auto viewport = Util::VK::makeViewport(m_swapChainExtent);
        const auto scissor  = Util::VK::makeRect(m_swapChainExtent);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRendering(commandBuffer);

    const auto postBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask       = VK_ACCESS_2_NONE,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swapImage,
        .subresourceRange    = Util::VK::Defaults::ImageSubresourceRange(),
    };
    const auto postDependency = VkDependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = 0,
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers    = nullptr,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &postBarrier,
    };
    vkCmdPipelineBarrier2(commandBuffer, &postDependency);
    checkVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}

auto VulkanRenderer::render() -> void {
    if (!m_window->isExposed()) {
        return;
    }
    initialize();
    if (m_swapChainDirty && !recreateSwapChain()) {
        return;
    }
    checkVk(vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX), "vkWaitForFences");

    auto       imageIndex    = uint32_t{};
    const auto acquireResult = vkAcquireNextImageKHR(
        m_device, m_swapChain, UINT64_MAX, m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapChainDirty = true;
        m_window->requestUpdate();
        return;
    }
    require(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR, "vkAcquireNextImageKHR failed");

    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        checkVk(vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX), "vkWaitForFences");
    }
    m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];
    checkVk(vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]), "vkResetFences");
    checkVk(vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0), "vkResetCommandBuffer");
    recordFrame(m_commandBuffers[m_currentFrame], imageIndex);

    const auto waitStage  = VkPipelineStageFlags{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const auto submitInfo = VkSubmitInfo{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = nullptr,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &m_imageAvailable[m_currentFrame],
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &m_commandBuffers[m_currentFrame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &m_renderFinished[imageIndex],
    };
    {
        auto queueLock = std::lock_guard<std::mutex>(m_rdpBackend->queueMutex());
        checkVk(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]), "vkQueueSubmit");
    }
    const auto presentInfo = VkPresentInfoKHR{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &m_renderFinished[imageIndex],
        .swapchainCount     = 1,
        .pSwapchains        = &m_swapChain,
        .pImageIndices      = &imageIndex,
        .pResults           = nullptr,
    };
    VkResult presentResult = VK_SUCCESS;
    {
        auto queueLock = std::lock_guard<std::mutex>(m_rdpBackend->queueMutex());
        presentResult  = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    }
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
        m_swapChainDirty = true;
        m_window->requestUpdate();
    } else {
        checkVk(presentResult, "vkQueuePresentKHR");
    }
    m_currentFrame = (m_currentFrame + 1) % FramesInFlight;
}

auto VulkanRenderer::markSwapChainDirty() -> void {
    m_swapChainDirty = true;
}

auto VulkanRenderer::shutdown() -> void {
    if (m_backendInitialized) {
        m_rdpBackend->destroy();
        m_backendInitialized = false;
    }
    if (m_device != VK_NULL_HANDLE) {
        checkVk(vkDeviceWaitIdle(m_device), "vkDeviceWaitIdle");
        destroySwapChainResources();
        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }
        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
        for (const auto semaphore : m_imageAvailable) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, semaphore, nullptr);
            }
        }
        for (const auto fence : m_inFlightFences) {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(m_device, fence, nullptr);
            }
        }
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    m_graphicsQueue  = VK_NULL_HANDLE;
    m_presentQueue   = VK_NULL_HANDLE;
    m_initialized    = false;
    m_swapChainDirty = true;
}

} // namespace GUI
