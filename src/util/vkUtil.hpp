#pragma once

#include <vulkan/vulkan.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <meta>
#include <print>
#include <ranges>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

namespace Util {
namespace VK {

namespace Defaults {
constexpr auto ImageSubresourceRange(VkImageAspectFlagBits bits = VK_IMAGE_ASPECT_COLOR_BIT) -> VkImageSubresourceRange {
    return VkImageSubresourceRange{
        .aspectMask     = bits,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };
}
constexpr auto ImageSubresourceLayers(VkImageAspectFlagBits bits = VK_IMAGE_ASPECT_COLOR_BIT) -> VkImageSubresourceLayers {
    return VkImageSubresourceLayers{
        .aspectMask     = bits,
        .mipLevel       = 0,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };
}
constexpr auto ComponentMapping = VkComponentMapping{
    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
};
constexpr auto SamplerCreateInfo = VkSamplerCreateInfo{
    .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext                   = nullptr,
    .flags                   = 0,
    .magFilter               = VK_FILTER_LINEAR,
    .minFilter               = VK_FILTER_LINEAR,
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
    .maxLod                  = 0.0f,
    .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_TRUE,
};

constexpr auto PipelineMultisampleStateCreateInfo = VkPipelineMultisampleStateCreateInfo{
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

constexpr auto PipelineColorBlendAttachmentState = VkPipelineColorBlendAttachmentState{
    .blendEnable         = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp        = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp        = VK_BLEND_OP_ADD,
    .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
};

constexpr auto PipelineDepthStencilStateCreateInfo = VkPipelineDepthStencilStateCreateInfo{
    .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .pNext                 = nullptr,
    .flags                 = 0,
    .depthTestEnable       = VK_TRUE,
    .depthWriteEnable      = VK_TRUE,
    .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable     = VK_FALSE,
    .front                 = {},
    .back                  = {},
    .minDepthBounds        = 0.0f,
    .maxDepthBounds        = 1.0f,
};

} // namespace Defaults

#if defined(ENABLE_VK_VALIDATION)
#define VK_TRY(x) \
    if (VkResult res = (x); res != VK_SUCCESS) throw Util::VK::Error(res);
#else
#define VK_TRY(x) (x)
#endif

struct Error : std::runtime_error {
    static constexpr auto resultName(VkResult result) -> std::string {
        constexpr static auto enums = std::define_static_array(std::meta::enumerators_of(^^VkResult));
        template for (constexpr auto e : enums) {
            if (result == [:e:]) {
                return std::string(std::meta::display_string_of(e));
            }
        }
        return std::format("VkResult({})", static_cast<int>(result));
    }

    template <typename... Args>
    Error(std::format_string<Args...> fmt, Args&&... args)
        : std::runtime_error(std::format(fmt, std::forward<Args>(args)...)) {}

    Error(VkResult result) : std::runtime_error(std::format("Vulkan error: {}", resultName(result))) {}
};

struct Device {
    VkDevice         device         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    auto valid() const -> bool {
        return device != VK_NULL_HANDLE && physicalDevice != VK_NULL_HANDLE;
    }
};

inline auto findMemoryType(VkDevice vkDevice, VkPhysicalDevice vkPhysDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t {
    auto memProperties = VkPhysicalDeviceMemoryProperties2{
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        .pNext            = nullptr,
        .memoryProperties = {},
    };
    vkGetPhysicalDeviceMemoryProperties2(vkPhysDevice, &memProperties);

    const auto& props = memProperties.memoryProperties;
    for (const auto i : std::views::iota(0u, props.memoryTypeCount)) {
        if ((typeFilter & (1 << i)) && (props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw Error("Failed to find suitable memory type!");
}

inline auto makeViewport(VkExtent2D extent) -> VkViewport {
    return VkViewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(extent.width),
        .height   = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
}

inline auto makeRect(VkExtent2D extent) -> VkRect2D {
    return VkRect2D{
        .offset = {0, 0},
        .extent = extent,
    };
}

inline auto makeViewportState(const VkViewport& viewport, const VkRect2D& scissor) -> VkPipelineViewportStateCreateInfo {
    return VkPipelineViewportStateCreateInfo{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor,
    };
}

inline auto makePipelineShaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits stage) -> VkPipelineShaderStageCreateInfo {
    return VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stage               = stage,
        .module              = module,
        .pName               = "main",
        .pSpecializationInfo = nullptr,
    };
}

inline auto hostBufferAllocationInfo(Device device, VkBuffer buffer) -> VkMemoryAllocateInfo {
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device.device, buffer, &requirements);
    const auto allocInfo = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = requirements.size,
        .memoryTypeIndex = findMemoryType(device.device, device.physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    return allocInfo;
}

inline auto makeTextureWriteDescriptorSet(VkDescriptorSet dstSet, uint32_t index, const VkDescriptorImageInfo& imageInfo) -> VkWriteDescriptorSet {
    return VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = dstSet,
        .dstBinding       = index,
        .dstArrayElement  = 0,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &imageInfo,
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr,
    };
}

inline auto makeBufferWriteDescriptorSet(VkDescriptorSet dstSet, uint32_t index, const VkDescriptorBufferInfo& bufferInfo) -> VkWriteDescriptorSet {
    return VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = dstSet,
        .dstBinding       = index,
        .dstArrayElement  = 0,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo       = nullptr,
        .pBufferInfo      = &bufferInfo,
        .pTexelBufferView = nullptr,
    };
}
struct AllocatedBuffer {
    Device         device{};
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void*          ptr    = nullptr;
    AllocatedBuffer()     = default;
    AllocatedBuffer(Device device) : device(device) {
    }
    void init(VkBufferUsageFlags usage, std::size_t size) {
        const auto info = VkBufferCreateInfo{
            .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .size                  = size,
            .usage                 = usage,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
        };
        VK_TRY(vkCreateBuffer(device.device, &info, nullptr, &(this->buffer)));

        const auto allocInfo = hostBufferAllocationInfo(device, buffer);
        VK_TRY(vkAllocateMemory(device.device, &allocInfo, nullptr, &(this->memory)));
        VK_TRY(vkBindBufferMemory(device.device, buffer, this->memory, 0));
        VK_TRY(vkMapMemory(device.device, this->memory, 0, size, 0, &(this->ptr)));
    }

    void destroy() {
        if (memory != VK_NULL_HANDLE) {
            vkUnmapMemory(device.device, this->memory);
            vkFreeMemory(device.device, this->memory, nullptr);
            this->memory = VK_NULL_HANDLE;
            this->ptr    = nullptr;
        }
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device.device, this->buffer, nullptr);
            this->buffer = VK_NULL_HANDLE;
        }
    }
};

inline auto deviceImageAllocationInfo(Device device, VkImage image) -> VkMemoryAllocateInfo {
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device.device, image, &requirements);
    const auto allocInfo = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = requirements.size,
        .memoryTypeIndex = findMemoryType(device.device, device.physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    return allocInfo;
}

inline auto createImageView(VkDevice vkDevice, VkImage image, VkFormat format, VkImageAspectFlagBits aspectMask, VkComponentMapping swizzle = Util::VK::Defaults::ComponentMapping) -> VkImageView {
    const auto viewInfo = VkImageViewCreateInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .image            = image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .components       = swizzle,
        .subresourceRange = Util::VK::Defaults::ImageSubresourceRange(aspectMask),
    };
    VkImageView imageView;
    VK_TRY(vkCreateImageView(vkDevice, &viewInfo, nullptr, &imageView));
    return imageView;
}

struct AllocatedImage {
    Device         device{};
    VkImage        image  = VK_NULL_HANDLE;
    VkImageView    view   = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    AllocatedImage() = default;

    AllocatedImage(Device device) : device(device) {}

    void init(VkImage image, VkFormat format, VkImageAspectFlagBits aspectMask, VkComponentMapping swizzle = Util::VK::Defaults::ComponentMapping) {
        const auto     allocInfo = deviceImageAllocationInfo(device, image);
        VkDeviceMemory mem;
        VK_TRY(vkAllocateMemory(device.device, &allocInfo, nullptr, &mem));
        VK_TRY(vkBindImageMemory(device.device, image, mem, 0));

        this->image  = image;
        this->view   = createImageView(device.device, image, format, aspectMask, swizzle);
        this->memory = mem;
    }

    void destroy() {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device.device, view, nullptr);
            view = VK_NULL_HANDLE;
        }
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(device.device, image, nullptr);
            image = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device.device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }
};

struct AllocatedTexture {
    Device                   device{};
    VkSampler                sampler = VK_NULL_HANDLE;
    Util::VK::AllocatedImage image{};

    AllocatedTexture() = default;
    AllocatedTexture(Device device) : device(device) {}

    void init(VkSamplerCreateInfo samplerCreateInfo, AllocatedImage image) {
        this->image = image;
        VK_TRY(vkCreateSampler(this->device.device, &samplerCreateInfo, nullptr, &this->sampler));
    }

    void destroy() {
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(this->device.device, this->sampler, nullptr);
            this->sampler = VK_NULL_HANDLE;
        }
        image.destroy();
    }
};

inline auto readSpirvShader(std::filesystem::path path) -> std::vector<uint32_t> {
    auto file = std::ifstream(path, std::ios::binary);
    if (!file.is_open()) {
        throw Error("Failed to open SPIR-V shader file!");
    }
    const auto size   = std::filesystem::file_size(path);
    auto       buffer = std::vector<uint32_t>(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

inline auto shaderModuleCreateInfo(const std::vector<uint32_t>& shaderCode) -> VkShaderModuleCreateInfo {
    return VkShaderModuleCreateInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .codeSize = shaderCode.size() * sizeof(uint32_t),
        .pCode    = shaderCode.data(),
    };
}

inline auto createTextureImage(Device device, VkFormat vkFormat, uint32_t width, uint32_t height, VkComponentMapping swizzle = Util::VK::Defaults::ComponentMapping) -> AllocatedImage {
    const auto imgInfo = VkImageCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = vkFormat,
        .extent                = VkExtent3D{.width = width, .height = height, .depth = 1},
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image;
    VK_TRY(vkCreateImage(device.device, &imgInfo, nullptr, &image));

    AllocatedImage allocatedImage(device);
    allocatedImage.init(image, vkFormat, VK_IMAGE_ASPECT_COLOR_BIT, swizzle);
    return allocatedImage;
}
inline auto createColourImage(Device device, VkExtent2D size, VkImageUsageFlags flags) -> AllocatedImage {
    const auto imgInfo = VkImageCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{.width = size.width, .height = size.height, .depth = 1},
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = flags,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image;
    VK_TRY(vkCreateImage(device.device, &imgInfo, nullptr, &image));

    AllocatedImage allocatedImage(device);
    allocatedImage.init(image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    return allocatedImage;
}

inline auto createDepthImage(Device device, VkExtent2D size) -> AllocatedImage {
    const auto depthInfo = VkImageCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_D32_SFLOAT,
        .extent                = VkExtent3D{.width = size.width, .height = size.height, .depth = 1},
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image;
    VK_TRY(vkCreateImage(device.device, &depthInfo, nullptr, &image));

    AllocatedImage allocatedImage(device);
    allocatedImage.init(image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
    return allocatedImage;
}

inline auto makeImageMemoryBarrier(AllocatedImage image, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlagBits bits) -> VkImageMemoryBarrier2 {
    return VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = srcStageMask,
        .srcAccessMask       = srcAccessMask,
        .dstStageMask        = dstStageMask,
        .dstAccessMask       = dstAccessMask,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image.image,
        .subresourceRange    = Util::VK::Defaults::ImageSubresourceRange(bits),
    };
}

inline auto addPipelineBarrier(VkCommandBuffer commandBuffer, VkImageMemoryBarrier2 barrier) -> void {
    const auto dependency = VkDependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = 0,
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers    = nullptr,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &barrier,
    };
    vkCmdPipelineBarrier2(commandBuffer, &dependency);
}

inline auto makeRenderingAttachmentInfo(VkImageView imageView, VkImageLayout imageLayout, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkClearValue clearValue) -> VkRenderingAttachmentInfo {
    return VkRenderingAttachmentInfo{
        .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext              = nullptr,
        .imageView          = imageView,
        .imageLayout        = imageLayout,
        .resolveMode        = VK_RESOLVE_MODE_NONE,
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp             = loadOp,
        .storeOp            = storeOp,
        .clearValue         = clearValue,
    };
}

inline auto resetCommandBuffer(VkCommandBuffer commandBuffer) -> void {
    vkResetCommandBuffer(commandBuffer, 0);
    const auto beginInfo = VkCommandBufferBeginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    VK_TRY(vkBeginCommandBuffer(commandBuffer, &beginInfo));
}

inline auto cmdCopyBufferToImage(VkCommandBuffer commandBuffer, AllocatedBuffer buffer, AllocatedImage image, VkExtent2D extent) -> void {
    const auto region = VkBufferImageCopy{
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = Defaults::ImageSubresourceLayers(),
        .imageOffset       = VkOffset3D{.x = 0, .y = 0, .z = 0},
        .imageExtent       = VkExtent3D{.width = extent.width, .height = extent.height, .depth = 1},
    };
    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer.buffer,
        image.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);
}

inline auto submitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkFence fence) -> void {
    const auto commandInfo = VkCommandBufferSubmitInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext         = nullptr,
        .commandBuffer = commandBuffer,
        .deviceMask    = 0,
    };
    const auto submitInfo = VkSubmitInfo2{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext                    = nullptr,
        .flags                    = 0,
        .waitSemaphoreInfoCount   = 0,
        .pWaitSemaphoreInfos      = nullptr,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &commandInfo,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos    = nullptr,
    };
    VK_TRY(vkEndCommandBuffer(commandBuffer));
    VK_TRY(vkQueueSubmit2(queue, 1, &submitInfo, fence));
}
} // namespace VK
} // namespace Util
