#include <ranges>
#include <cstring>
#include <print>
#include <vector>
#include <util/vkUtil.hpp>
#include <QImage>
#include "vulkanBackend.hpp"

namespace RDP {

auto VulkanBackend::init(
    VkDevice         vkDevice,
    VkInstance       vkInstance,
    VkPhysicalDevice vkPhysicalDevice,
    uint32_t         vkQueueFamilyIndex) -> void {
    m_vkInstance         = vkInstance;
    m_vkPhysicalDevice   = vkPhysicalDevice;
    m_vkDevice           = vkDevice;
    m_vkQueueFamilyIndex = vkQueueFamilyIndex;

    vkGetDeviceQueue(m_vkDevice, m_vkQueueFamilyIndex, 0, &m_vkQueue);

    createRenderTargets();
    createPipeline();
    createCmdObjects();
}

auto VulkanBackend::destroy() -> void {
    vkDeviceWaitIdle(m_vkDevice);
    //  TODO vertex buffer

    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkUnmapMemory(m_vkDevice, m_vertexBufferMem);
        m_vertexBufferMapped = nullptr;
        vkDestroyBuffer(m_vkDevice, m_vertexBuffer, nullptr);
        m_vertexBuffer = VK_NULL_HANDLE;
        vkFreeMemory(m_vkDevice, m_vertexBufferMem, nullptr);
        m_vertexBufferMem = VK_NULL_HANDLE;
    }
    vkDestroyFence(m_vkDevice, m_renderFence, nullptr);
    vkDestroyCommandPool(m_vkDevice, m_commandPool, nullptr);
    vkDestroyPipeline(m_vkDevice, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_vkDevice, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(m_vkDevice, m_renderPass, nullptr);
    for (const auto i : std::views::iota(0uz, MAX_BUFFERS)) {
        vkDestroyImageView(m_vkDevice, m_renderTargets[i].m_imageView, nullptr);
        vkDestroyImage(m_vkDevice, m_renderTargets[i].m_image, nullptr);
        vkFreeMemory(m_vkDevice, m_renderTargets[i].m_mem, nullptr);
    }
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
            .extent                = VkExtent3D{.width = 320, .height = 240, .depth = 1}, // todo
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
        .stride    = 3 * sizeof(int32_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    const auto attributeDescriptions = std::array<VkVertexInputAttributeDescription, 1>{
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SINT,
            .offset   = 0,
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
        .width    = 320,
        .height   = 240,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const auto scissor = VkRect2D{
        .offset = {0, 0},
        .extent = {320, 240},
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

    const auto rasterization = VkPipelineRasterizationStateCreateInfo{
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
        .blendEnable         = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
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
        .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    const auto layoutInfo = VkPipelineLayoutCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 0,
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = nullptr,
    };
    vkCreatePipelineLayout(m_vkDevice, &layoutInfo, nullptr, &m_pipelineLayout);

    const auto colourFormat        = VK_FORMAT_R8G8B8A8_UNORM;
    const auto renderingCreateInfo = VkPipelineRenderingCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                   = nullptr,
        .viewMask                = 0,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &colourFormat,
        .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
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
        .pDepthStencilState  = nullptr,
        .pColorBlendState    = &blendState,
        .pDynamicState       = nullptr,
        .layout              = m_pipelineLayout,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
    };

    vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_pipeline);
    vkDestroyShaderModule(m_vkDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(m_vkDevice, fragShaderModule, nullptr);
}

auto VulkanBackend::renderFrame() -> void {
    if (m_vertexData.empty()) {
        return;
    }

    reallocVertexBuffer(m_vertexData.size() * sizeof(int32_t));
    std::memcpy(m_vertexBufferMapped, m_vertexData.data(), m_vertexData.size() * sizeof(int32_t));

    vkWaitForFences(m_vkDevice, 1, &m_renderFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_vkDevice, 1, &m_renderFence);
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
        .dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
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

    const auto colourAttachment = VkRenderingAttachmentInfo{
        .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext              = nullptr,
        .imageView          = m_renderTargets[m_renderTargetWriteIndex].m_imageView,
        .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode        = VK_RESOLVE_MODE_NONE,
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue         = VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
    };

    const auto vkRenderingInfo = VkRenderingInfo{
        .sType      = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext      = nullptr,
        .flags      = 0,
        .renderArea = VkRect2D{
            .offset = VkOffset2D{.x = 0, .y = 0},
            .extent = VkExtent2D{.width = 320, .height = 240},
        },
        .layerCount           = 1,
        .viewMask             = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colourAttachment,
        .pDepthAttachment     = nullptr,
        .pStencilAttachment   = nullptr,
    };

    vkCmdBeginRendering(m_commandBuffer, &vkRenderingInfo);
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    auto offset = VkDeviceSize{0};
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_vertexBuffer, &offset);
    vkCmdDraw(m_commandBuffer, m_vertexData.size() / 3, 1, 0, 0);
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
    m_renderTargetReadIndex.store(m_renderTargetWriteIndex, std::memory_order_release);
    m_renderTargetWriteIndex = (m_renderTargetWriteIndex + 1) % m_renderTargets.size();
    m_renderedAtLeastOnce    = true;
    m_vertexData.clear();

    // {
    //     auto lock = std::lock_guard<std::mutex>(m_queueMutex);
    //     dumpToFile();
    //     std::terminate();
    // }
}

auto VulkanBackend::dumpToFile() -> void {
    const auto     image     = m_renderTargets[m_renderTargetReadIndex.load(std::memory_order_acquire)].m_image;
    constexpr auto imageSize = VkDeviceSize{320 * 240 * 4};

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
        .imageExtent = VkExtent3D{.width = 320, .height = 240, .depth = 1},
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
    QImage qimg(static_cast<uchar*>(data), 320, 240, qFormat);
    qimg.save("output.png");
    vkUnmapMemory(m_vkDevice, stagingMemory);
    vkDestroyBuffer(m_vkDevice, stagingBuffer, nullptr);
    vkFreeMemory(m_vkDevice, stagingMemory, nullptr);
}

auto VulkanBackend::getRenderOutput() -> RenderOutput {
    if (!m_renderedAtLeastOnce)
        return RenderOutput{.m_image = VK_NULL_HANDLE, .m_imageView = VK_NULL_HANDLE, .m_extent = VkExtent2D{.width = 0, .height = 0}};
    const auto& rt = m_renderTargets[m_renderTargetReadIndex.load(std::memory_order_acquire)];
    return RenderOutput{
        .m_image     = rt.m_image,
        .m_imageView = rt.m_imageView,
        .m_extent    = VkExtent2D{.width = 320, .height = 240}};
}
auto VulkanBackend::addTriangle(const int32_t* vtxs) -> void {
    for (const auto i : std::views::iota(0, 3)) {
        m_vertexData.insert(m_vertexData.end(), {vtxs[i * 3], vtxs[i * 3 + 1], vtxs[i * 3 + 2]});
    }
}

} // namespace RDP