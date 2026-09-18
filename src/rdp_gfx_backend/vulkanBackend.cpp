#include <ranges>
#include <cstring>
#include <optional>
#include <bit>
#include <span>
#include <vector>
#include <util/vkUtil.hpp>
#include <QImage>
#include "vulkanBackend.hpp"

namespace RDP {

static auto nativeFormatFor(TextureFormat format) -> std::optional<VulkanTextureFormat> {
    constexpr auto identity = VkComponentMapping{
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    switch (format) {
        case TextureFormat::RGBA16:
            return VulkanTextureFormat{
                .vkFormat      = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                .swizzle       = identity,
                .bytesPerTexel = 2};
        case TextureFormat::RGBA32:
            return VulkanTextureFormat{
                .vkFormat      = VK_FORMAT_R8G8B8A8_UNORM,
                .swizzle       = identity,
                .bytesPerTexel = 4};
        case TextureFormat::IA16:
            return VulkanTextureFormat{
                .vkFormat      = VK_FORMAT_R8G8_UNORM,
                .swizzle       = VkComponentMapping{.r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_R, .b = VK_COMPONENT_SWIZZLE_R, .a = VK_COMPONENT_SWIZZLE_G},
                .bytesPerTexel = 2,
            };
        case TextureFormat::I8:
            return VulkanTextureFormat{
                .vkFormat      = VK_FORMAT_R8_UNORM,
                .swizzle       = VkComponentMapping{.r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_R, .b = VK_COMPONENT_SWIZZLE_R, .a = VK_COMPONENT_SWIZZLE_R},
                .bytesPerTexel = 1,
            };
        default:
            return std::nullopt;
    }
}

// resolves palette (CI4/CI8), sub-byte-packed (IA4/I4), and YUV16 formats to plain RGBA8
// on the CPU so the result can still be sampled (and bilinear-filtered) natively
static auto decodeToRGBA32(std::vector<std::byte>& textureMemory,
                           const TileParams&       params,
                           const std::byte*        texelData,
                           TextureFormat           paletteFormat,
                           const std::byte*        paletteData) -> void {
    const auto readUint4 = [&](const std::byte* data, uint32_t x, uint32_t y) -> uint8_t {
        const auto byte = static_cast<uint8_t>(data[y * params.stride + x / 2]);
        return (x % 2 == 0) ? (byte >> 4) : (byte & 0xF);
    };
    const auto readUint8 = [&](const std::byte* data, uint32_t x, uint32_t y) -> uint8_t {
        return static_cast<uint8_t>(data[y * params.stride + x]);
    };
    const auto lookupPalette = [&](uint8_t index) -> std::array<uint8_t, 4> {
        auto texel = (reinterpret_cast<const uint16_t*>(paletteData))[index * 4];
        if (std::endian::native == std::endian::little) {
            texel = std::byteswap(texel);
        }

        if (paletteFormat == TextureFormat::RGBA16) {
            const auto r = static_cast<uint8_t>((texel >> 11) & 0x1F);
            const auto g = static_cast<uint8_t>((texel >> 6) & 0x1F);
            const auto b = static_cast<uint8_t>((texel >> 1) & 0x1F);
            const auto a = static_cast<uint8_t>(texel & 0x1);
            return {
                static_cast<uint8_t>(r << 3 | r >> 2),
                static_cast<uint8_t>(g << 3 | g >> 2),
                static_cast<uint8_t>(b << 3 | b >> 2),
                static_cast<uint8_t>(a * 255),
            };
        } else if (paletteFormat == TextureFormat::IA16) {
            const auto i = static_cast<uint8_t>(texel >> 8);
            const auto a = static_cast<uint8_t>(texel & 0xFF);
            return {i, i, i, a};
        } else [[unlikely]] {
            throw std::runtime_error("Unsupported palette format");
        }
    };

    textureMemory.clear();
    for (const auto y : std::views::iota(0u, params.height)) {
        for (const auto x : std::views::iota(0u, params.width)) {
            auto rgba = std::array<uint8_t, 4>{0, 0, 0, 255};
            switch (params.format) {
                case TextureFormat::CI4: {
                    rgba = lookupPalette((params.subPalette << 4) | readUint4(texelData, x, y));
                    break;
                }
                case TextureFormat::CI8: {
                    rgba = lookupPalette(readUint8(texelData, x, y));
                    break;
                }
                case TextureFormat::IA4: {
                    const auto nibble    = readUint4(texelData, x, y);
                    const auto intensity = static_cast<uint8_t>((nibble >> 1) * 255 / 7);
                    const auto alpha     = static_cast<uint8_t>((nibble & 1) * 255);
                    rgba                 = {intensity, intensity, intensity, alpha};
                    break;
                }
                case TextureFormat::IA8: {
                    const auto texel     = static_cast<uint8_t>(texelData[y * params.stride + x]);
                    const auto intensity = static_cast<uint8_t>((texel >> 4) * 17);
                    const auto alpha     = static_cast<uint8_t>((texel & 0xF) * 17);
                    rgba                 = {intensity, intensity, intensity, alpha};
                    break;
                }
                case TextureFormat::I4: {
                    const auto nibble    = readUint4(texelData, x, y);
                    const auto intensity = static_cast<uint8_t>(nibble << 4);
                    rgba                 = {intensity, intensity, intensity, intensity};
                    break;
                }
                case TextureFormat::YUV16: {
                    // TODO proper YUV->RGB conversion
                    const auto luma = readUint8(texelData, 2 * x, y);
                    rgba            = {luma, luma, luma, 255};
                    break;
                }
                default:
                    break;
            }
            for (const auto component : rgba) {
                textureMemory.push_back(std::byte{component});
            }
        }
    }
}

auto VulkanBackend::init(
    VkDevice         vkDevice,
    VkInstance       vkInstance,
    VkPhysicalDevice vkPhysicalDevice,
    uint32_t         vkQueueFamilyIndex) -> void {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);

    m_vkInstance         = vkInstance;
    m_vkPhysicalDevice   = vkPhysicalDevice;
    m_vkDevice           = vkDevice;
    m_vkQueueFamilyIndex = vkQueueFamilyIndex;
    m_currentRenderPass  = {};
    vkGetDeviceQueue(m_vkDevice, m_vkQueueFamilyIndex, 0, &m_vkQueue);

    createRenderTargets();
    createDescriptorSetLayout();
    createPipeline();
    createCmdObjects();
    createDescriptorPool();
    createFallbackTexture();
    createTileParamsBuffer();
    createDescriptorSet();

    m_renderedAtLeastOnce = false;
    m_initialized         = true;
}

auto VulkanBackend::destroy() -> void {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
    if (!m_initialized) {
        return;
    }

    m_initialized = false;
    vkDeviceWaitIdle(m_vkDevice);

    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkUnmapMemory(m_vkDevice, m_vertexBufferMem);
        vkDestroyBuffer(m_vkDevice, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
        vkFreeMemory(m_vkDevice, m_vertexBufferMem, nullptr);
        m_vertexBufferMem = VK_NULL_HANDLE;
    }
    m_vertexBufferMapped = nullptr;
    if (m_tileParamsBuffer != VK_NULL_HANDLE) {
        vkUnmapMemory(m_vkDevice, m_tileParamsBufferMem);
        vkDestroyBuffer(m_vkDevice, m_tileParamsBuffer, nullptr);
        vkFreeMemory(m_vkDevice, m_tileParamsBufferMem, nullptr);
        m_tileParamsBuffer       = VK_NULL_HANDLE;
        m_tileParamsBufferMem    = VK_NULL_HANDLE;
        m_tileParamsBufferMapped = nullptr;
    }
    vkDestroyFence(m_vkDevice, m_renderFence, nullptr);
    m_renderFence = VK_NULL_HANDLE;
    vkDestroyCommandPool(m_vkDevice, m_commandPool, nullptr);
    m_commandPool   = VK_NULL_HANDLE;
    m_commandBuffer = VK_NULL_HANDLE;
    vkDestroyPipeline(m_vkDevice, m_pipeline, nullptr);
    m_pipeline = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(m_vkDevice, m_pipelineLayout, nullptr);
    m_pipelineLayout = VK_NULL_HANDLE;
    vkDestroyRenderPass(m_vkDevice, m_renderPass, nullptr);
    m_renderPass = VK_NULL_HANDLE;
    for (const auto i : std::views::iota(0uz, NUM_TILES)) {
        vkDestroySampler(m_vkDevice, m_textures[i].sampler, nullptr);
        m_textures[i].sampler = VK_NULL_HANDLE;
        vkDestroyImageView(m_vkDevice, m_textures[i].imageView, nullptr);
        m_textures[i].imageView = VK_NULL_HANDLE;
        vkDestroyImage(m_vkDevice, m_textures[i].image, nullptr);
        m_textures[i].image = VK_NULL_HANDLE;
        vkFreeMemory(m_vkDevice, m_textures[i].imageMem, nullptr);
        m_textures[i].imageMem = VK_NULL_HANDLE;
    }
    vkDestroySampler(m_vkDevice, m_fallbackTexture.sampler, nullptr);
    vkDestroyImageView(m_vkDevice, m_fallbackTexture.imageView, nullptr);
    vkDestroyImage(m_vkDevice, m_fallbackTexture.image, nullptr);
    vkFreeMemory(m_vkDevice, m_fallbackTexture.imageMem, nullptr);
    m_fallbackTexture = {};
    vkDestroyDescriptorPool(m_vkDevice, m_textureDescriptorPool, nullptr);
    m_textureDescriptorPool = VK_NULL_HANDLE;
    m_textureDescriptorSet  = VK_NULL_HANDLE;
    vkDestroyDescriptorSetLayout(m_vkDevice, m_textureDescriptorSetLayout, nullptr);
    m_textureDescriptorSetLayout = VK_NULL_HANDLE;
    for (const auto i : std::views::iota(0uz, MAX_BUFFERS)) {
        vkDestroyImageView(m_vkDevice, m_renderTargets[i].m_imageView, nullptr);
        vkDestroyImage(m_vkDevice, m_renderTargets[i].m_image, nullptr);
        vkFreeMemory(m_vkDevice, m_renderTargets[i].m_mem, nullptr);
        vkDestroyImageView(m_vkDevice, m_renderTargets[i].m_depthImageView, nullptr);
        vkDestroyImage(m_vkDevice, m_renderTargets[i].m_depthImage, nullptr);
        vkFreeMemory(m_vkDevice, m_renderTargets[i].m_depthMem, nullptr);
        m_renderTargets[i] = {};
    }
    m_vertexBufferSize = 0;
    m_currentRenderPass.reset();
    m_renderedAtLeastOnce      = false;
    m_currentRenderPass.active = false;
    m_renderTargetWriteIndex   = 0;
    m_renderTargetReadIndex.store(0, std::memory_order_release);
    m_vkQueue            = VK_NULL_HANDLE;
    m_vkDevice           = VK_NULL_HANDLE;
    m_vkPhysicalDevice   = VK_NULL_HANDLE;
    m_vkInstance         = VK_NULL_HANDLE;
    m_vkQueueFamilyIndex = 0;
}

auto VulkanBackend::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_vkPhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
};

auto VulkanBackend::reallocVertexBuffer(std::size_t newSize) -> void {
    if (m_vertexBuffer != VK_NULL_HANDLE && m_vertexBufferSize >= newSize) {
        return;
    }
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkUnmapMemory(m_vkDevice, m_vertexBufferMem);
        vkDestroyBuffer(m_vkDevice, m_vertexBuffer, nullptr);
        vkFreeMemory(m_vkDevice, m_vertexBufferMem, nullptr);
    }
    m_vertexBufferSize = newSize * 2;
    const auto bufInfo = VkBufferCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .size                  = m_vertexBufferSize,
        .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
    };
    vkCreateBuffer(m_vkDevice, &bufInfo, nullptr, &m_vertexBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_vkDevice, m_vertexBuffer, &memRequirements);

    auto allocInfo = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &m_vertexBufferMem);
    vkBindBufferMemory(m_vkDevice, m_vertexBuffer, m_vertexBufferMem, 0);
    vkMapMemory(m_vkDevice, m_vertexBufferMem, 0, m_vertexBufferSize, 0, &m_vertexBufferMapped);
}

auto VulkanBackend::createRenderTargets() -> void {
    for (const auto i : std::views::iota(0uz, MAX_BUFFERS)) {
        const auto imgInfo = VkImageCreateInfo{
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = VK_FORMAT_R8G8B8A8_UNORM,
            .extent                = VkExtent3D{.width = m_extent.width, .height = m_extent.height, .depth = 1},
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_LINEAR,
            .usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        vkCreateImage(m_vkDevice, &imgInfo, nullptr, &m_renderTargets[i].m_image);

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(m_vkDevice, m_renderTargets[i].m_image, &memReq);

        const auto allocInfo = VkMemoryAllocateInfo{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = memReq.size,
            .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &m_renderTargets[i].m_mem);
        vkBindImageMemory(m_vkDevice, m_renderTargets[i].m_image, m_renderTargets[i].m_mem, 0);

        const auto viewInfo = VkImageViewCreateInfo{
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext      = nullptr,
            .flags      = 0,
            .image      = m_renderTargets[i].m_image,
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = VK_FORMAT_R8G8B8A8_UNORM,
            .components = VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = VkImageSubresourceRange{
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &m_renderTargets[i].m_imageView);

        const auto depthInfo = VkImageCreateInfo{
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = VK_FORMAT_D32_SFLOAT,
            .extent                = VkExtent3D{.width = m_extent.width, .height = m_extent.height, .depth = 1},
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
        vkCreateImage(m_vkDevice, &depthInfo, nullptr, &m_renderTargets[i].m_depthImage);

        VkMemoryRequirements depthMemReq;
        vkGetImageMemoryRequirements(m_vkDevice, m_renderTargets[i].m_depthImage, &depthMemReq);
        const auto depthAllocInfo = VkMemoryAllocateInfo{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = depthMemReq.size,
            .memoryTypeIndex = findMemoryType(depthMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        vkAllocateMemory(m_vkDevice, &depthAllocInfo, nullptr, &m_renderTargets[i].m_depthMem);
        vkBindImageMemory(m_vkDevice, m_renderTargets[i].m_depthImage, m_renderTargets[i].m_depthMem, 0);

        const auto depthViewInfo = VkImageViewCreateInfo{
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext      = nullptr,
            .flags      = 0,
            .image      = m_renderTargets[i].m_depthImage,
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = VK_FORMAT_D32_SFLOAT,
            .components = VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = VkImageSubresourceRange{
                .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        vkCreateImageView(m_vkDevice, &depthViewInfo, nullptr, &m_renderTargets[i].m_depthImageView);
    }
}

auto VulkanBackend::createCmdObjects() -> void {
    const auto poolInfo = VkCommandPoolCreateInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_vkQueueFamilyIndex,
    };
    vkCreateCommandPool(m_vkDevice, &poolInfo, nullptr, &m_commandPool);

    const auto allocInfo = VkCommandBufferAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = m_commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(m_vkDevice, &allocInfo, &m_commandBuffer);

    const auto fenceInfo = VkFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &m_renderFence);
}

auto VulkanBackend::createDescriptorSetLayout() -> void {
    auto bindings = std::array<VkDescriptorSetLayoutBinding, NUM_TILES + 1>{};
    for (uint32_t i = 0; i < NUM_TILES; ++i) {
        bindings[i] = VkDescriptorSetLayoutBinding{
            .binding            = i,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        };
    }
    bindings[NUM_TILES] = VkDescriptorSetLayoutBinding{
        .binding            = static_cast<uint32_t>(NUM_TILES),
        .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr,
    };
    const auto layoutInfo = VkDescriptorSetLayoutCreateInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data(),
    };
    vkCreateDescriptorSetLayout(m_vkDevice, &layoutInfo, nullptr, &m_textureDescriptorSetLayout);
}

auto VulkanBackend::createDescriptorPool() -> void {
    const auto poolSizes = std::array{
        VkDescriptorPoolSize{
            .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = NUM_TILES,
        },
        VkDescriptorPoolSize{
            .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        },
    };
    const auto poolInfo = VkDescriptorPoolCreateInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .maxSets       = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data(),
    };
    vkCreateDescriptorPool(m_vkDevice, &poolInfo, nullptr, &m_textureDescriptorPool);
}

auto VulkanBackend::createTileParamsBuffer() -> void {
    const auto bufferInfo = VkBufferCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .size                  = sizeof(m_currentRenderPass.tileParams),
        .usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
    };
    vkCreateBuffer(m_vkDevice, &bufferInfo, nullptr, &m_tileParamsBuffer);

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(m_vkDevice, m_tileParamsBuffer, &requirements);
    const auto allocation = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = requirements.size,
        .memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    vkAllocateMemory(m_vkDevice, &allocation, nullptr, &m_tileParamsBufferMem);
    vkBindBufferMemory(m_vkDevice, m_tileParamsBuffer, m_tileParamsBufferMem, 0);
    vkMapMemory(m_vkDevice, m_tileParamsBufferMem, 0, bufferInfo.size, 0, &m_tileParamsBufferMapped);
    std::memcpy(m_tileParamsBufferMapped, m_currentRenderPass.tileParams.data(), sizeof(m_currentRenderPass.tileParams));
}

auto VulkanBackend::createFallbackTexture() -> void {
    const auto imageInfo = VkImageCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = VkExtent3D{.width = 1, .height = 1, .depth = 1},
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
    vkCreateImage(m_vkDevice, &imageInfo, nullptr, &m_fallbackTexture.image);

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(m_vkDevice, m_fallbackTexture.image, &requirements);
    const auto allocation = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = requirements.size,
        .memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    vkAllocateMemory(m_vkDevice, &allocation, nullptr, &m_fallbackTexture.imageMem);
    vkBindImageMemory(m_vkDevice, m_fallbackTexture.image, m_fallbackTexture.imageMem, 0);

    const auto fallbackPixel = std::array<std::byte, 4>{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    const auto stagingInfo   = VkBufferCreateInfo{
          .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .pNext                 = nullptr,
          .flags                 = 0,
          .size                  = fallbackPixel.size(),
          .usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
          .queueFamilyIndexCount = 0,
          .pQueueFamilyIndices   = nullptr,
    };
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    vkCreateBuffer(m_vkDevice, &stagingInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements stagingRequirements;
    vkGetBufferMemoryRequirements(m_vkDevice, stagingBuffer, &stagingRequirements);
    const auto stagingAllocation = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = stagingRequirements.size,
        .memoryTypeIndex = findMemoryType(stagingRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    vkAllocateMemory(m_vkDevice, &stagingAllocation, nullptr, &stagingMemory);
    vkBindBufferMemory(m_vkDevice, stagingBuffer, stagingMemory, 0);

    void* mapped = nullptr;
    vkMapMemory(m_vkDevice, stagingMemory, 0, fallbackPixel.size(), 0, &mapped);
    std::memcpy(mapped, fallbackPixel.data(), fallbackPixel.size());
    vkUnmapMemory(m_vkDevice, stagingMemory);

    const auto samplerInfo = VkSamplerCreateInfo{
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
    vkCreateSampler(m_vkDevice, &samplerInfo, nullptr, &m_fallbackTexture.sampler);

    const auto viewInfo = VkImageViewCreateInfo{
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext      = nullptr,
        .flags      = 0,
        .image      = m_fallbackTexture.image,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = VK_FORMAT_R8G8B8A8_UNORM,
        .components = VkComponentMapping{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &m_fallbackTexture.imageView);

    vkResetCommandBuffer(m_commandBuffer, 0);
    vkResetFences(m_vkDevice, 1, &m_renderFence);
    const auto beginInfo = VkCommandBufferBeginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
    const auto preBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask       = VK_ACCESS_2_NONE,
        .dstStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_fallbackTexture.image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
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
    vkCmdPipelineBarrier2(m_commandBuffer, &preDependency);

    const auto region = VkBufferImageCopy{
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = VkImageSubresourceLayers{
             .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
             .mipLevel       = 0,
             .baseArrayLayer = 0,
             .layerCount     = 1,
        },
        .imageOffset = VkOffset3D{.x = 0, .y = 0, .z = 0},
        .imageExtent = VkExtent3D{.width = 1, .height = 1, .depth = 1},
    };
    vkCmdCopyBufferToImage(
        m_commandBuffer,
        stagingBuffer,
        m_fallbackTexture.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);

    const auto postBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_fallbackTexture.image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
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
    vkCmdPipelineBarrier2(m_commandBuffer, &postDependency);
    vkEndCommandBuffer(m_commandBuffer);

    const auto commandInfo = VkCommandBufferSubmitInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext         = nullptr,
        .commandBuffer = m_commandBuffer,
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
    {
        auto queueLock = std::lock_guard<std::mutex>(m_queueMutex);
        vkQueueSubmit2(m_vkQueue, 1, &submitInfo, m_renderFence);
    }
    vkWaitForFences(m_vkDevice, 1, &m_renderFence, VK_TRUE, UINT64_MAX);
    vkDestroyBuffer(m_vkDevice, stagingBuffer, nullptr);
    vkFreeMemory(m_vkDevice, stagingMemory, nullptr);
}

auto VulkanBackend::createDescriptorSet() -> void {
    const auto allocation = VkDescriptorSetAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = nullptr,
        .descriptorPool     = m_textureDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &m_textureDescriptorSetLayout,
    };
    vkAllocateDescriptorSets(m_vkDevice, &allocation, &m_textureDescriptorSet);

    const auto imageInfo = VkDescriptorImageInfo{
        .sampler     = m_fallbackTexture.sampler,
        .imageView   = m_fallbackTexture.imageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    auto imageInfos = std::array<VkDescriptorImageInfo, NUM_TILES>{};
    imageInfos.fill(imageInfo);
    auto writes = std::array<VkWriteDescriptorSet, NUM_TILES + 1>{};
    for (uint32_t i = 0; i < NUM_TILES; ++i) {
        writes[i] = VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = m_textureDescriptorSet,
            .dstBinding       = i,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &imageInfos[i],
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        };
    }
    const auto tileParamsInfo = VkDescriptorBufferInfo{
        .buffer = m_tileParamsBuffer,
        .offset = 0,
        .range  = sizeof(m_currentRenderPass.tileParams),
    };
    writes[NUM_TILES] = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = m_textureDescriptorSet,
        .dstBinding       = static_cast<uint32_t>(NUM_TILES),
        .dstArrayElement  = 0,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo       = nullptr,
        .pBufferInfo      = &tileParamsInfo,
        .pTexelBufferView = nullptr,
    };
    vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

auto VulkanBackend::createPipeline() -> void {
    const auto     vertShader     = Util::VK::readSpirvShader("rdp.vert.spv");
    const auto     vertCreateInfo = Util::VK::shaderModuleCreateInfo(vertShader);
    VkShaderModule vertShaderModule;
    vkCreateShaderModule(m_vkDevice, &vertCreateInfo, nullptr, &vertShaderModule);

    const auto     fragShader     = Util::VK::readSpirvShader("rdp.frag.spv");
    const auto     fragCreateInfo = Util::VK::shaderModuleCreateInfo(fragShader);
    VkShaderModule fragShaderModule;
    vkCreateShaderModule(m_vkDevice, &fragCreateInfo, nullptr, &fragShaderModule);

    const auto pipelineShaderStages = std::array<VkPipelineShaderStageCreateInfo, 2>{
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
        }};

    const auto bindingDescription = VkVertexInputBindingDescription{
        .binding   = 0,
        .stride    = 11 * sizeof(int32_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    const auto attributeDescriptions = std::array<VkVertexInputAttributeDescription, 4>{
        // tile
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding  = 0,
            .format   = VK_FORMAT_R32_SINT,
            .offset   = 0,
        },
        // position
        VkVertexInputAttributeDescription{
            .location = 1,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SINT,
            .offset   = sizeof(uint32_t),
        },
        // UV texture coordinates
        VkVertexInputAttributeDescription{
            .location = 2,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SINT,
            .offset   = 4 * sizeof(int32_t),
        },
        // color
        VkVertexInputAttributeDescription{
            .location = 3,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32A32_SINT,
            .offset   = 7 * sizeof(int32_t),
        },
    };

    const auto vertexInputInfo = VkPipelineVertexInputStateCreateInfo{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions    = attributeDescriptions.data(),
    };
    const auto inputAssembly = VkPipelineInputAssemblyStateCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    const auto viewport = VkViewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(m_extent.width),
        .height   = static_cast<float>(m_extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const auto scissor = VkRect2D{
        .offset = {0, 0},
        .extent = m_extent,
    };

    const auto viewportState = VkPipelineViewportStateCreateInfo{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor,
    };

    const auto conservativeRasterization = VkPipelineRasterizationConservativeStateCreateInfoEXT{
        .sType                            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
        .pNext                            = nullptr,
        .flags                            = 0,
        .conservativeRasterizationMode    = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT,
        .extraPrimitiveOverestimationSize = 0.0f,
    };

    const auto rasterization = VkPipelineRasterizationStateCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                   = &conservativeRasterization,
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

    const auto multisampling = VkPipelineMultisampleStateCreateInfo{
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

    const auto blendAttachment = VkPipelineColorBlendAttachmentState{
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const auto blendState = VkPipelineColorBlendStateCreateInfo{
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &blendAttachment,
        .blendConstants  = {1.0f, 1.0f, 1.0f, .0f},
    };

    const auto pushRange = VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(RDP::VulkanBackend::RdpRenderPassConstants),
    };

    const auto layoutInfo = VkPipelineLayoutCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1,
        .pSetLayouts            = &m_textureDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushRange,
    };
    vkCreatePipelineLayout(m_vkDevice, &layoutInfo, nullptr, &m_pipelineLayout);

    const auto colourFormat        = VK_FORMAT_R8G8B8A8_UNORM;
    const auto renderingCreateInfo = VkPipelineRenderingCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                   = nullptr,
        .viewMask                = 0,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &colourFormat,
        .depthAttachmentFormat   = VK_FORMAT_D32_SFLOAT,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    const auto dynamicStates    = std::array<VkDynamicState, 1>{VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT};
    const auto dynamicStateInfo = VkPipelineDynamicStateCreateInfo{
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext             = nullptr,
        .flags             = 0,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates    = dynamicStates.data(),
    };

    const auto pipelineCreateInfo = VkGraphicsPipelineCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &renderingCreateInfo,
        .flags               = 0,
        .stageCount          = 2,
        .pStages             = pipelineShaderStages.data(),
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterization,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = [] {
            static const auto state = VkPipelineDepthStencilStateCreateInfo{
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
            return &state;
        }(),
        .pColorBlendState   = &blendState,
        .pDynamicState      = &dynamicStateInfo,
        .layout             = m_pipelineLayout,
        .renderPass         = VK_NULL_HANDLE,
        .subpass            = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex  = -1,
    };

    vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_pipeline);
    vkDestroyShaderModule(m_vkDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(m_vkDevice, fragShaderModule, nullptr);
}

auto VulkanBackend::startRenderPass(RenderOptions options) -> void {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
    if (!m_initialized || m_currentRenderPass.vertexData.empty()) {
        return;
    }

    vkWaitForFences(m_vkDevice, 1, &m_renderFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_vkDevice, 1, &m_renderFence);

    std::memcpy(m_tileParamsBufferMapped, m_currentRenderPass.tileParams.data(), sizeof(m_currentRenderPass.tileParams));
    reallocVertexBuffer(m_currentRenderPass.vertexData.size() * sizeof(int32_t));
    std::memcpy(m_vertexBufferMapped, m_currentRenderPass.vertexData.data(), m_currentRenderPass.vertexData.size() * sizeof(int32_t));
    vkResetCommandBuffer(m_commandBuffer, 0);

    const auto beginInfo = VkCommandBufferBeginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

    const auto preBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = m_currentRenderPass.active ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask       = m_currentRenderPass.active ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_NONE,
        .dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout           = m_currentRenderPass.active ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_renderTargets[m_renderTargetWriteIndex].m_image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };

    const auto preDepInfo = VkDependencyInfo{
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
    vkCmdPipelineBarrier2(m_commandBuffer, &preDepInfo);

    const auto depthBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = m_currentRenderPass.active ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask       = m_currentRenderPass.active ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_NONE,
        .dstStageMask        = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .dstAccessMask       = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout           = m_currentRenderPass.active ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_renderTargets[m_renderTargetWriteIndex].m_depthImage,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };
    const auto depthDepInfo = VkDependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = 0,
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers    = nullptr,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &depthBarrier,
    };
    vkCmdPipelineBarrier2(m_commandBuffer, &depthDepInfo);

    const auto colourAttachment = VkRenderingAttachmentInfo{
        .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext              = nullptr,
        .imageView          = m_renderTargets[m_renderTargetWriteIndex].m_imageView,
        .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode        = VK_RESOLVE_MODE_NONE,
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp             = m_currentRenderPass.active ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue         = VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    const auto depthAttachment = VkRenderingAttachmentInfo{
        .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext              = nullptr,
        .imageView          = m_renderTargets[m_renderTargetWriteIndex].m_depthImageView,
        .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .resolveMode        = VK_RESOLVE_MODE_NONE,
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp             = m_currentRenderPass.active ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue         = VkClearValue{.depthStencil = {.depth = 1.0f, .stencil = 0}},
    };

    const auto vkRenderingInfo = VkRenderingInfo{
        .sType      = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext      = nullptr,
        .flags      = 0,
        .renderArea = VkRect2D{
            .offset = VkOffset2D{.x = 0, .y = 0},
            .extent = m_extent,
        },
        .layerCount           = 1,
        .viewMask             = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colourAttachment,
        .pDepthAttachment     = &depthAttachment,
        .pStencilAttachment   = nullptr,
    };

    vkCmdBeginRendering(m_commandBuffer, &vkRenderingInfo);
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    if (m_textureDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_textureDescriptorSet, 0, nullptr);
    }
    auto offset = VkDeviceSize{0};
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_vertexBuffer, &offset);
    vkCmdPushConstants(m_commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RdpRenderPassConstants), &m_currentRenderPass.pushConstants);
    vkCmdSetDepthTestEnable(m_commandBuffer, options.depthTestEnable ? VK_TRUE : VK_FALSE);
    vkCmdSetDepthWriteEnable(m_commandBuffer, options.depthWriteEnable ? VK_TRUE : VK_FALSE);
    vkCmdDraw(m_commandBuffer, m_currentRenderPass.vertexData.size() / 10, 1, 0, 0);
    vkCmdSetDepthTestEnable(m_commandBuffer, VK_TRUE);
    vkCmdSetDepthWriteEnable(m_commandBuffer, VK_TRUE);
    vkCmdEndRendering(m_commandBuffer);

    const auto postBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_renderTargets[m_renderTargetWriteIndex].m_image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };
    const auto postDepInfo = VkDependencyInfo{
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
    vkCmdPipelineBarrier2(m_commandBuffer, &postDepInfo);

    vkEndCommandBuffer(m_commandBuffer);

    const auto cmdSubmitInfo = VkCommandBufferSubmitInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext         = nullptr,
        .commandBuffer = m_commandBuffer,
        .deviceMask    = 0,
    };

    const auto submitInfo = VkSubmitInfo2{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext                    = nullptr,
        .flags                    = 0,
        .waitSemaphoreInfoCount   = 0,
        .pWaitSemaphoreInfos      = nullptr,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmdSubmitInfo,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos    = nullptr,
    };
    {
        auto lock = std::lock_guard<std::mutex>(m_queueMutex);
        vkQueueSubmit2(m_vkQueue, 1, &submitInfo, m_renderFence);
    }
    m_currentRenderPass.active = true;
    m_currentRenderPass.vertexData.clear();
}

auto VulkanBackend::completeRenderFrame() -> void {
    {
        auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
        if (!m_initialized || !m_currentRenderPass.active) {
            return;
        }

        m_renderTargetReadIndex.store(m_renderTargetWriteIndex, std::memory_order_release);
        m_renderTargetWriteIndex   = (m_renderTargetWriteIndex + 1) % m_renderTargets.size();
        m_currentRenderPass.active = false;

        // if (!m_renderedAtLeastOnce) {
        //     auto lock = std::lock_guard<std::mutex>(m_queueMutex);
        //     dumpToFile();
        // }
        m_renderedAtLeastOnce = true;
    }
    notifyFrameComplete();
}

auto VulkanBackend::setFrameCompleteCallback(FrameCompleteCallback callback, void* userData) -> void {
    auto lock                 = std::lock_guard<std::mutex>(m_callbackMutex);
    m_frameCompleteCallback   = callback;
    m_frameCompleteUserData   = userData;
}

auto VulkanBackend::notifyFrameComplete() -> void {
    auto callback = FrameCompleteCallback{};
    auto userData = static_cast<void*>(nullptr);
    {
        auto lock = std::lock_guard<std::mutex>(m_callbackMutex);
        callback  = m_frameCompleteCallback;
        userData  = m_frameCompleteUserData;
    }
    if (callback) {
        callback(userData);
    }
}

auto VulkanBackend::dumpToFile() -> void {
    const auto image     = m_renderTargets[m_renderTargetReadIndex.load(std::memory_order_acquire)].m_image;
    const auto imageSize = VkDeviceSize{m_extent.width * m_extent.height * 4};

    VkBuffer        stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory  stagingMemory = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    const auto bufferInfo = VkBufferCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .size                  = imageSize,
        .usage                 = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
    };
    vkCreateBuffer(m_vkDevice, &bufferInfo, nullptr, &stagingBuffer);

    auto memReqs = VkMemoryRequirements{};
    vkGetBufferMemoryRequirements(m_vkDevice, stagingBuffer, &memReqs);

    const auto allocInfo = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(m_vkDevice, stagingBuffer, stagingMemory, 0);

    const auto cmdAlloc = VkCommandBufferAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = m_commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(m_vkDevice, &cmdAlloc, &commandBuffer);

    const auto beginInfo = VkCommandBufferBeginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    const auto preBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask       = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };

    const auto preDep = VkDependencyInfo{
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
    vkCmdPipelineBarrier2(commandBuffer, &preDep);
    const auto region = VkBufferImageCopy{
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = VkImageSubresourceLayers{
             .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
             .mipLevel       = 0,
             .baseArrayLayer = 0,
             .layerCount     = 1,
        },
        .imageOffset = VkOffset3D{.x = 0, .y = 0, .z = 0},
        .imageExtent = VkExtent3D{.width = m_extent.width, .height = m_extent.height, .depth = 1},
    };
    vkCmdCopyImageToBuffer(
        commandBuffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        stagingBuffer,
        1,
        &region);

    const auto postBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };

    const auto hostReadBarrier = VkBufferMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask       = VK_ACCESS_2_HOST_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = stagingBuffer,
        .offset              = 0,
        .size                = imageSize,
    };

    const auto postDep = VkDependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = 0,
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers    = &hostReadBarrier,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &postBarrier,
    };
    vkCmdPipelineBarrier2(commandBuffer, &postDep);

    vkEndCommandBuffer(commandBuffer);

    const auto submitInfo = VkSubmitInfo{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = nullptr,
        .waitSemaphoreCount   = 0,
        .pWaitSemaphores      = nullptr,
        .pWaitDstStageMask    = nullptr,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores    = nullptr,
    };
    const auto fenceInfo = VkFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VkFence dumpFence = VK_NULL_HANDLE;
    vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &dumpFence);
    vkQueueSubmit(m_vkQueue, 1, &submitInfo, dumpFence);
    vkWaitForFences(m_vkDevice, 1, &dumpFence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_vkDevice, dumpFence, nullptr);
    vkFreeCommandBuffers(m_vkDevice, m_commandPool, 1, &commandBuffer);
    void* data = nullptr;
    vkMapMemory(m_vkDevice, stagingMemory, 0, VK_WHOLE_SIZE, 0, &data);
    auto   qFormat = QImage::Format_RGBA8888;
    QImage qimg(static_cast<uchar*>(data), m_extent.width, m_extent.height, qFormat);
    qimg.save("output.png");
    vkUnmapMemory(m_vkDevice, stagingMemory);
    vkDestroyBuffer(m_vkDevice, stagingBuffer, nullptr);
    vkFreeMemory(m_vkDevice, stagingMemory, nullptr);
}

auto VulkanBackend::getRenderOutput() -> RenderOutput {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
    if (!m_initialized || !m_renderedAtLeastOnce)
        return RenderOutput{.m_image = VK_NULL_HANDLE, .m_imageView = VK_NULL_HANDLE, .m_extent = VkExtent2D{.width = 0, .height = 0}};
    const auto& rt = m_renderTargets[m_renderTargetReadIndex.load(std::memory_order_acquire)];
    return RenderOutput{
        .m_image     = rt.m_image,
        .m_imageView = rt.m_imageView,
        .m_extent    = m_extent,
    };
}

auto VulkanBackend::updateTile(std::size_t      index,
                               TileParams       params,
                               const std::byte* texelData,
                               TextureFormat    paletteFormat,
                               const std::byte* paletteData) -> void {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
    if (!m_initialized || index >= NUM_TILES || params.width == 0 || params.height == 0) {
        return;
    }
    m_currentRenderPass.tileParams[index] = ShaderTileInfo{
        .extent = {params.width, params.height, params.stride, static_cast<uint32_t>(params.format)},
        .s      = params.s,
        .t      = params.t,
    };

    vkWaitForFences(m_vkDevice, 1, &m_renderFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_vkDevice, 1, &m_renderFence);

    auto vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
    auto swizzle  = VkComponentMapping{
         .r = VK_COMPONENT_SWIZZLE_IDENTITY,
         .g = VK_COMPONENT_SWIZZLE_IDENTITY,
         .b = VK_COMPONENT_SWIZZLE_IDENTITY,
         .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    };

    if (const auto native = nativeFormatFor(params.format)) {
        vkFormat = native->vkFormat;
        swizzle  = native->swizzle;
        m_textureMemory.resize(static_cast<std::size_t>(params.width) * params.height * native->bytesPerTexel);
        for (const auto y : std::views::iota(0u, params.height)) {
            const auto rowBytes = static_cast<std::size_t>(params.width) * native->bytesPerTexel;
            const auto dst      = m_textureMemory.data() + y * rowBytes;
            const auto src      = texelData + y * params.stride;
            std::memcpy(dst, src, rowBytes);
            if (std::endian::native == std::endian::little && params.format == TextureFormat::RGBA16) {
                const auto texels = reinterpret_cast<uint16_t*>(dst);
                for (const auto x : std::views::iota(0u, params.width)) {
                    texels[x] = std::byteswap(texels[x]);
                }
            }
        }
    } else {
        decodeToRGBA32(m_textureMemory,
                       params,
                       texelData,
                       paletteFormat,
                       paletteData);
    }
    vkDestroySampler(m_vkDevice, m_textures[index].sampler, nullptr);
    m_textures[index].sampler = VK_NULL_HANDLE;
    vkDestroyImageView(m_vkDevice, m_textures[index].imageView, nullptr);
    m_textures[index].imageView = VK_NULL_HANDLE;
    vkDestroyImage(m_vkDevice, m_textures[index].image, nullptr);
    m_textures[index].image = VK_NULL_HANDLE;
    vkFreeMemory(m_vkDevice, m_textures[index].imageMem, nullptr);
    m_textures[index].imageMem = VK_NULL_HANDLE;

    const auto imgInfo = VkImageCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = vkFormat,
        .extent                = VkExtent3D{.width = params.width, .height = params.height, .depth = 1},
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
    vkCreateImage(m_vkDevice, &imgInfo, nullptr, &m_textures[index].image);

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_vkDevice, m_textures[index].image, &memReq);
    const auto allocInfo = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memReq.size,
        .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &m_textures[index].imageMem);
    vkBindImageMemory(m_vkDevice, m_textures[index].image, m_textures[index].imageMem, 0);

    const auto samplerInfo = VkSamplerCreateInfo{
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
    vkCreateSampler(m_vkDevice, &samplerInfo, nullptr, &m_textures[index].sampler);

    const auto stagingInfo = VkBufferCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .size                  = m_textureMemory.size(),
        .usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
    };
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    vkCreateBuffer(m_vkDevice, &stagingInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements stagingRequirements;
    vkGetBufferMemoryRequirements(m_vkDevice, stagingBuffer, &stagingRequirements);
    const auto stagingAllocInfo = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = stagingRequirements.size,
        .memoryTypeIndex = findMemoryType(stagingRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    vkAllocateMemory(m_vkDevice, &stagingAllocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(m_vkDevice, stagingBuffer, stagingMemory, 0);

    void* stagingMapped = nullptr;
    vkMapMemory(m_vkDevice, stagingMemory, 0, m_textureMemory.size(), 0, &stagingMapped);
    std::memcpy(stagingMapped, m_textureMemory.data(), m_textureMemory.size());
    vkUnmapMemory(m_vkDevice, stagingMemory);

    vkResetCommandBuffer(m_commandBuffer, 0);
    const auto beginInfo = VkCommandBufferBeginInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

    const auto preBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask       = VK_ACCESS_2_NONE,
        .dstStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_textures[index].image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };
    const auto preDepInfo = VkDependencyInfo{
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
    vkCmdPipelineBarrier2(m_commandBuffer, &preDepInfo);

    const auto region = VkBufferImageCopy{
        .bufferOffset      = 0,
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = VkImageSubresourceLayers{
             .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
             .mipLevel       = 0,
             .baseArrayLayer = 0,
             .layerCount     = 1,
        },
        .imageOffset = VkOffset3D{.x = 0, .y = 0, .z = 0},
        .imageExtent = VkExtent3D{.width = params.width, .height = params.height, .depth = 1},
    };
    vkCmdCopyBufferToImage(
        m_commandBuffer,
        stagingBuffer,
        m_textures[index].image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);

    const auto postBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = m_textures[index].image,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };
    const auto postDepInfo = VkDependencyInfo{
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
    vkCmdPipelineBarrier2(m_commandBuffer, &postDepInfo);
    vkEndCommandBuffer(m_commandBuffer);

    const auto cmdSubmitInfo = VkCommandBufferSubmitInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext         = nullptr,
        .commandBuffer = m_commandBuffer,
        .deviceMask    = 0,
    };
    const auto submitInfo = VkSubmitInfo2{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext                    = nullptr,
        .flags                    = 0,
        .waitSemaphoreInfoCount   = 0,
        .pWaitSemaphoreInfos      = nullptr,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmdSubmitInfo,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos    = nullptr,
    };
    {
        auto qLock = std::lock_guard<std::mutex>(m_queueMutex);
        vkQueueSubmit2(m_vkQueue, 1, &submitInfo, m_renderFence);
    }
    vkWaitForFences(m_vkDevice, 1, &m_renderFence, VK_TRUE, UINT64_MAX);

    vkDestroyBuffer(m_vkDevice, stagingBuffer, nullptr);
    vkFreeMemory(m_vkDevice, stagingMemory, nullptr);

    const auto viewInfo = VkImageViewCreateInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .image            = m_textures[index].image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = vkFormat,
        .components       = swizzle,
        .subresourceRange = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &m_textures[index].imageView);

    if (m_textureDescriptorSet == VK_NULL_HANDLE) {
        const auto allocInfo = VkDescriptorSetAllocateInfo{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = m_textureDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &m_textureDescriptorSetLayout,
        };
        vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &m_textureDescriptorSet);
    }

    const auto imageInfo = VkDescriptorImageInfo{
        .sampler     = m_textures[index].sampler,
        .imageView   = m_textures[index].imageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const auto write = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = m_textureDescriptorSet,
        .dstBinding       = static_cast<uint32_t>(index),
        .dstArrayElement  = 0,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &imageInfo,
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr,
    };
    vkUpdateDescriptorSets(m_vkDevice, 1, &write, 0, nullptr);
}

auto VulkanBackend::addTriangle(uint32_t         tile,
                                const std::byte* vtxBytes,
                                const std::byte* shadeBytes,
                                const std::byte* uvBytes) -> void {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
    if (!m_initialized) {
        return;
    }
    if (tile >= NUM_TILES) {
        return;
    }
    const auto vtxs   = reinterpret_cast<const int32_t*>(vtxBytes);
    const auto shades = reinterpret_cast<const int32_t*>(shadeBytes);
    const auto uvs    = reinterpret_cast<const int32_t*>(uvBytes);

    for (const auto i : std::views::iota(0, 3)) {
        m_currentRenderPass.vertexData.insert(
            m_currentRenderPass.vertexData.end(),
            {
                static_cast<int32_t>(tile),

                vtxs[i * 3],
                vtxs[i * 3 + 1],
                vtxs[i * 3 + 2],

                uvs ? uvs[i * 3] : 0,
                uvs ? uvs[i * 3 + 1] : 0,
                uvs ? uvs[i * 3 + 2] : 0,

                // RGBA
                shades ? shades[i * 3] : 0,
                shades ? shades[i * 3 + 1] : 0,
                shades ? shades[i * 3 + 2] : 0,
                shades ? shades[i * 3 + 3] : 0,
            });
    }
}

auto VulkanBackend::setCombineInputs(const CombineInputs& inputs) -> void {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
    if (!m_initialized) {
        return;
    }
    m_currentRenderPass.pushConstants.combineInputs = inputs;
}

} // namespace RDP