#include <ranges>
#include <cstring>
#include <optional>
#include <bit>
#include <vector>
#include <util/vkUtil.hpp>
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

    VK_TRY(vkWaitForFences(m_device.device, 1, &m_renderFence, VK_TRUE, UINT64_MAX));
    VK_TRY(vkResetFences(m_device.device, 1, &m_renderFence));

    auto vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
    auto swizzle  = Util::VK::Defaults::ComponentMapping;

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
    m_textures[index].destroy();

    const auto image       = createTextureImage(m_device, vkFormat, params.width, params.height, swizzle);
    const auto samplerInfo = Util::VK::Defaults::SamplerCreateInfo;
    m_textures[index].init(samplerInfo, image);

    auto stagingBuffer = Util::VK::AllocatedBuffer(m_device);
    stagingBuffer.init(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_textureMemory.size());
    std::memcpy(stagingBuffer.ptr, m_textureMemory.data(), m_textureMemory.size());

    Util::VK::resetCommandBuffer(m_commandBuffer);

    Util::VK::addPipelineBarrier(m_commandBuffer,
                                 Util::VK::makeImageMemoryBarrier(
                                     m_textures[index].image,
                                     VK_PIPELINE_STAGE_2_NONE,
                                     VK_ACCESS_2_NONE,
                                     VK_PIPELINE_STAGE_2_COPY_BIT,
                                     VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT));
    Util::VK::cmdCopyBufferToImage(m_commandBuffer, stagingBuffer, m_textures[index].image, VkExtent2D{.width = params.width, .height = params.height});

    Util::VK::addPipelineBarrier(m_commandBuffer,
                                 Util::VK::makeImageMemoryBarrier(
                                     m_textures[index].image,
                                     VK_PIPELINE_STAGE_2_COPY_BIT,
                                     VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                     VK_ACCESS_2_SHADER_READ_BIT,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT));

    {
        auto qLock = std::lock_guard<std::mutex>(m_queueMutex);
        Util::VK::submitCommandBuffer(m_commandBuffer, m_vkQueue, m_renderFence);
    }
    VK_TRY(vkWaitForFences(m_device.device, 1, &m_renderFence, VK_TRUE, UINT64_MAX));
    stagingBuffer.destroy();

    const auto imageInfo = VkDescriptorImageInfo{
        .sampler     = m_textures[index].sampler,
        .imageView   = m_textures[index].image.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const auto write = Util::VK::makeTextureWriteDescriptorSet(m_textureDescriptorSet, static_cast<uint32_t>(index), imageInfo);
    vkUpdateDescriptorSets(m_device.device, 1, &write, 0, nullptr);
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
                shades ? shades[i * 4] : 0,
                shades ? shades[i * 4 + 1] : 0,
                shades ? shades[i * 4 + 2] : 0,
                shades ? shades[i * 4 + 3] : 0,
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

auto VulkanBackend::startRenderPass(RenderOptions options) -> void {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
    if (!m_initialized || m_currentRenderPass.vertexData.empty()) {
        return;
    }

    VK_TRY(vkWaitForFences(m_device.device, 1, &m_renderFence, VK_TRUE, UINT64_MAX));
    VK_TRY(vkResetFences(m_device.device, 1, &m_renderFence));

    std::memcpy(m_tileParamsBuffer.ptr, m_currentRenderPass.tileParams.data(), sizeof(m_currentRenderPass.tileParams));
    reallocVertexBuffer(m_currentRenderPass.vertexData.size() * sizeof(int32_t));
    std::memcpy(m_vertexBuffer.ptr, m_currentRenderPass.vertexData.data(), m_currentRenderPass.vertexData.size() * sizeof(int32_t));

    Util::VK::resetCommandBuffer(m_commandBuffer);

    Util::VK::addPipelineBarrier(m_commandBuffer,
                                 Util::VK::makeImageMemoryBarrier(
                                     m_renderTargets[m_renderTargetWriteIndex].colour,
                                     m_currentRenderPass.active ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_2_NONE,
                                     m_currentRenderPass.active ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_NONE,
                                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                     m_currentRenderPass.active ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT));

    Util::VK::addPipelineBarrier(m_commandBuffer,
                                 Util::VK::makeImageMemoryBarrier(
                                     m_renderTargets[m_renderTargetWriteIndex].depth,
                                     m_currentRenderPass.active ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_2_NONE,
                                     m_currentRenderPass.active ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_NONE,
                                     VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                     m_currentRenderPass.active ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_ASPECT_DEPTH_BIT));

    const auto colourAttachment = Util::VK::makeRenderingAttachmentInfo(
        m_renderTargets[m_renderTargetWriteIndex].colour.view,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        m_currentRenderPass.active ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});

    const auto depthAttachment = Util::VK::makeRenderingAttachmentInfo(
        m_renderTargets[m_renderTargetWriteIndex].depth.view,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        m_currentRenderPass.active ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VkClearValue{.depthStencil = {.depth = 1.0f, .stencil = 0}});

    const auto vkRenderingInfo = VkRenderingInfo{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext                = nullptr,
        .flags                = 0,
        .renderArea           = Util::VK::makeRect(m_extent),
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
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_vertexBuffer.buffer, &offset);
    vkCmdPushConstants(m_commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RdpRenderPassConstants), &m_currentRenderPass.pushConstants);
    vkCmdSetDepthTestEnable(m_commandBuffer, options.depthTestEnable ? VK_TRUE : VK_FALSE);
    vkCmdSetDepthWriteEnable(m_commandBuffer, options.depthWriteEnable ? VK_TRUE : VK_FALSE);
    vkCmdDraw(m_commandBuffer, m_currentRenderPass.vertexData.size() / 10, 1, 0, 0);
    vkCmdSetDepthTestEnable(m_commandBuffer, VK_TRUE);
    vkCmdSetDepthWriteEnable(m_commandBuffer, VK_TRUE);
    vkCmdEndRendering(m_commandBuffer);

    Util::VK::addPipelineBarrier(m_commandBuffer,
                                 Util::VK::makeImageMemoryBarrier(
                                     m_renderTargets[m_renderTargetWriteIndex].colour,
                                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                                     VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_ASPECT_COLOR_BIT));
    Util::VK::submitCommandBuffer(m_commandBuffer, m_vkQueue, m_renderFence);
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
        m_renderedAtLeastOnce      = true;
    }

    // fire callback
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

auto VulkanBackend::setFrameCompleteCallback(FrameCompleteCallback callback, void* userData) -> void {
    auto lock               = std::lock_guard<std::mutex>(m_callbackMutex);
    m_frameCompleteCallback = callback;
    m_frameCompleteUserData = userData;
}

auto VulkanBackend::init(
    VkDevice         vkDevice,
    VkInstance       vkInstance,
    VkPhysicalDevice vkPhysicalDevice,
    uint32_t         vkQueueFamilyIndex) -> void {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);

    m_vkInstance = vkInstance;
    m_device     = {
            .device         = vkDevice,
            .physicalDevice = vkPhysicalDevice};
    m_vkQueueFamilyIndex = vkQueueFamilyIndex;
    m_currentRenderPass  = {};
    vkGetDeviceQueue(m_device.device, m_vkQueueFamilyIndex, 0, &m_vkQueue);
    m_vertexBuffer.device = m_device;

    createRenderTargets();
    createDescriptorInfo();
    createPipeline();
    createCommandBuffer();
    createTextures();
    createDescriptorSets();

    m_renderedAtLeastOnce = false;
    m_initialized         = true;
}

auto VulkanBackend::createRenderTargets() -> void {
    constexpr auto colourImageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    for (const auto i : std::views::iota(0uz, MAX_BUFFERS)) {
        m_renderTargets[i].colour = Util::VK::createColourImage(m_device, m_extent, colourImageFlags);
        m_renderTargets[i].depth  = Util::VK::createDepthImage(m_device, m_extent);
    }
}

auto VulkanBackend::createDescriptorInfo() -> void {
    // descriptor set layout
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
    VK_TRY(vkCreateDescriptorSetLayout(m_device.device, &layoutInfo, nullptr, &m_textureDescriptorSetLayout));

    // descriptor pool
    const auto poolSizes = std::array{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = NUM_TILES},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
    };
    const auto poolInfo = VkDescriptorPoolCreateInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .maxSets       = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data(),
    };
    VK_TRY(vkCreateDescriptorPool(m_device.device, &poolInfo, nullptr, &m_textureDescriptorPool));
}

auto VulkanBackend::createPipeline() -> void {
    const auto     vertShader     = Util::VK::readSpirvShader("rdp.vert.spv");
    const auto     vertCreateInfo = Util::VK::shaderModuleCreateInfo(vertShader);
    VkShaderModule vertShaderModule;
    VK_TRY(vkCreateShaderModule(m_device.device, &vertCreateInfo, nullptr, &vertShaderModule));

    const auto     fragShader     = Util::VK::readSpirvShader("rdp.frag.spv");
    const auto     fragCreateInfo = Util::VK::shaderModuleCreateInfo(fragShader);
    VkShaderModule fragShaderModule;
    VK_TRY(vkCreateShaderModule(m_device.device, &fragCreateInfo, nullptr, &fragShaderModule));

    const auto pipelineShaderStages = std::array<VkPipelineShaderStageCreateInfo, 2>{
        Util::VK::makePipelineShaderStageCreateInfo(vertShaderModule, VK_SHADER_STAGE_VERTEX_BIT),
        Util::VK::makePipelineShaderStageCreateInfo(fragShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT)};

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

    const auto bindingDescription = VkVertexInputBindingDescription{
        .binding   = 0,
        .stride    = 11 * sizeof(int32_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
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

    const auto multisampling = Util::VK::Defaults::PipelineMultisampleStateCreateInfo;

    const auto blendAttachment = Util::VK::Defaults::PipelineColorBlendAttachmentState;

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
    VK_TRY(vkCreatePipelineLayout(m_device.device, &layoutInfo, nullptr, &m_pipelineLayout));

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

    const auto dynamicStates = std::array<VkDynamicState, 2>{
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT};
    const auto dynamicStateInfo = VkPipelineDynamicStateCreateInfo{
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext             = nullptr,
        .flags             = 0,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates    = dynamicStates.data(),
    };

    const auto viewport      = Util::VK::makeViewport(m_extent);
    const auto scissor       = Util::VK::makeRect(m_extent);
    const auto viewportState = Util::VK::makeViewportState(viewport, scissor);

    const auto depthStencilState  = Util::VK::Defaults::PipelineDepthStencilStateCreateInfo;
    const auto pipelineCreateInfo = VkGraphicsPipelineCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &renderingCreateInfo,
        .flags               = 0,
        .stageCount          = static_cast<uint32_t>(pipelineShaderStages.size()),
        .pStages             = pipelineShaderStages.data(),
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterization,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depthStencilState,
        .pColorBlendState    = &blendState,
        .pDynamicState       = &dynamicStateInfo,
        .layout              = m_pipelineLayout,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
    };

    VK_TRY(vkCreateGraphicsPipelines(m_device.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(m_device.device, fragShaderModule, nullptr);
}

auto VulkanBackend::createCommandBuffer() -> void {
    const auto poolInfo = VkCommandPoolCreateInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_vkQueueFamilyIndex,
    };
    VK_TRY(vkCreateCommandPool(m_device.device, &poolInfo, nullptr, &m_commandPool));

    const auto allocInfo = VkCommandBufferAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = m_commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_TRY(vkAllocateCommandBuffers(m_device.device, &allocInfo, &m_commandBuffer));

    const auto fenceInfo = VkFenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VK_TRY(vkCreateFence(m_device.device, &fenceInfo, nullptr, &m_renderFence));
}

auto VulkanBackend::createTextures() -> void {
    createDefaultTexture();
    for (const auto i : std::views::iota(0uz, NUM_TILES)) {
        m_textures[i] = Util::VK::AllocatedTexture(m_device);
    }
    m_tileParamsBuffer = Util::VK::AllocatedBuffer(m_device);
    m_tileParamsBuffer.init(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(m_currentRenderPass.tileParams));
}

auto VulkanBackend::destroy() -> void {
    if (!m_initialized) {
        return;
    }
    auto rlock = std::lock_guard<std::mutex>(m_resourceMutex);
    auto qlock = std::lock_guard<std::mutex>(m_queueMutex);

    m_initialized = false;
    VK_TRY(vkDeviceWaitIdle(m_device.device));

    m_vertexBuffer.destroy();
    m_tileParamsBuffer.destroy();
    vkDestroyFence(m_device.device, m_renderFence, nullptr);
    m_renderFence = VK_NULL_HANDLE;
    vkDestroyCommandPool(m_device.device, m_commandPool, nullptr);
    m_commandPool   = VK_NULL_HANDLE;
    m_commandBuffer = VK_NULL_HANDLE;
    vkDestroyPipeline(m_device.device, m_pipeline, nullptr);
    m_pipeline = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(m_device.device, m_pipelineLayout, nullptr);
    m_pipelineLayout = VK_NULL_HANDLE;
    vkDestroyRenderPass(m_device.device, m_renderPass, nullptr);
    m_renderPass = VK_NULL_HANDLE;
    for (const auto i : std::views::iota(0uz, NUM_TILES)) {
        m_textures[i].destroy();
    }
    m_fallbackTexture.destroy();
    vkDestroyDescriptorPool(m_device.device, m_textureDescriptorPool, nullptr);
    m_textureDescriptorPool = VK_NULL_HANDLE;
    m_textureDescriptorSet  = VK_NULL_HANDLE;
    vkDestroyDescriptorSetLayout(m_device.device, m_textureDescriptorSetLayout, nullptr);
    m_textureDescriptorSetLayout = VK_NULL_HANDLE;
    for (const auto i : std::views::iota(0uz, MAX_BUFFERS)) {
        m_renderTargets[i].colour.destroy();
        m_renderTargets[i].depth.destroy();
    }
    m_vertexBufferSize = 0;
    m_currentRenderPass.reset();
    m_renderedAtLeastOnce      = false;
    m_currentRenderPass.active = false;
    m_renderTargetWriteIndex   = 0;
    m_renderTargetReadIndex.store(0, std::memory_order_release);
    m_vkQueue               = VK_NULL_HANDLE;
    m_device.device         = VK_NULL_HANDLE;
    m_device.physicalDevice = VK_NULL_HANDLE;
    m_vkInstance            = VK_NULL_HANDLE;
    m_vkQueueFamilyIndex    = 0;
}

auto VulkanBackend::createDescriptorSets() -> void {
    const auto allocation = VkDescriptorSetAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = nullptr,
        .descriptorPool     = m_textureDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &m_textureDescriptorSetLayout,
    };
    VK_TRY(vkAllocateDescriptorSets(m_device.device, &allocation, &m_textureDescriptorSet));

    const auto imageInfo = VkDescriptorImageInfo{
        .sampler     = m_fallbackTexture.sampler,
        .imageView   = m_fallbackTexture.image.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    auto imageInfos = std::array<VkDescriptorImageInfo, NUM_TILES>{};
    imageInfos.fill(imageInfo);
    auto writes = std::array<VkWriteDescriptorSet, NUM_TILES + 1>{};
    for (uint32_t i = 0; i < NUM_TILES; ++i) {
        writes[i] = Util::VK::makeTextureWriteDescriptorSet(m_textureDescriptorSet, i, imageInfos[i]);
    }
    const auto tileParamsInfo = VkDescriptorBufferInfo{
        .buffer = m_tileParamsBuffer.buffer,
        .offset = 0,
        .range  = sizeof(m_currentRenderPass.tileParams),
    };
    writes[NUM_TILES] = Util::VK::makeBufferWriteDescriptorSet(m_textureDescriptorSet, static_cast<uint32_t>(NUM_TILES), tileParamsInfo);
    vkUpdateDescriptorSets(m_device.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

auto VulkanBackend::getRenderOutput() -> RenderOutput {
    auto lock = std::lock_guard<std::mutex>(m_resourceMutex);
    if (!m_initialized || !m_renderedAtLeastOnce)
        return RenderOutput{.m_image = VK_NULL_HANDLE, .m_imageView = VK_NULL_HANDLE, .m_extent = VkExtent2D{.width = 0, .height = 0}};
    const auto& rt = m_renderTargets[m_renderTargetReadIndex.load(std::memory_order_acquire)];
    return RenderOutput{
        .m_image     = rt.colour.image,
        .m_imageView = rt.colour.view,
        .m_extent    = m_extent,
    };
}

auto VulkanBackend::createDefaultTexture() -> void {
    const auto fallbackPixel = std::array<std::byte, 4>{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};

    auto stagingBuffer = Util::VK::AllocatedBuffer(m_device);
    stagingBuffer.init(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, fallbackPixel.size());
    std::memcpy(stagingBuffer.ptr, fallbackPixel.data(), fallbackPixel.size());

    const auto image       = createColourImage(m_device, VkExtent2D{1, 1}, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto samplerInfo = Util::VK::Defaults::SamplerCreateInfo;
    m_fallbackTexture      = Util::VK::AllocatedTexture(m_device);
    m_fallbackTexture.init(samplerInfo, image);

    VK_TRY(vkResetFences(m_device.device, 1, &m_renderFence));
    Util::VK::resetCommandBuffer(m_commandBuffer);

    Util::VK::addPipelineBarrier(m_commandBuffer, Util::VK::makeImageMemoryBarrier(m_fallbackTexture.image, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT));

    Util::VK::cmdCopyBufferToImage(m_commandBuffer, stagingBuffer, m_fallbackTexture.image, VkExtent2D{.width = 1, .height = 1});

    Util::VK::addPipelineBarrier(m_commandBuffer, Util::VK::makeImageMemoryBarrier(m_fallbackTexture.image, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT));
    {
        auto queueLock = std::lock_guard<std::mutex>(m_queueMutex);
        Util::VK::submitCommandBuffer(m_commandBuffer, m_vkQueue, m_renderFence);
        VK_TRY(vkWaitForFences(m_device.device, 1, &m_renderFence, VK_TRUE, UINT64_MAX));
    }
    stagingBuffer.destroy();
}

auto VulkanBackend::reallocVertexBuffer(std::size_t newSize) -> void {
    if (m_vertexBuffer.buffer != VK_NULL_HANDLE && m_vertexBufferSize >= newSize) {
        return;
    }
    m_vertexBuffer.destroy();
    m_vertexBufferSize = newSize * 2;
    m_vertexBuffer.init(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBufferSize);
}

} // namespace RDP