#include <vulkan/vulkan.h>
#include <util/vkUtil.hpp>
#include "vkRenderWindow.hpp"

namespace GUI {

auto VulkanRenderer::initResources() -> void {

    m_rdpBackend->init(
        m_window->device(), m_window->vulkanInstance()->vkInstance(), m_window->physicalDevice(), m_window->graphicsQueueFamilyIndex());

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
    vkCreateSampler(m_window->device(), &samplerInfo, nullptr, &m_sampler);

    // descriptor sets
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
    vkCreateDescriptorSetLayout(m_window->device(), &descriptorSetLayoutInfo, nullptr, &m_descriptorSetLayout);

    // present pipelines

    const auto     vertShader = Util::VK::readSpirvShader(std::filesystem::path("window.vert.spv"));
    const auto     vertInfo   = Util::VK::shaderModuleCreateInfo(vertShader);
    VkShaderModule vertShaderModule;
    vkCreateShaderModule(m_window->device(), &vertInfo, nullptr, &vertShaderModule);

    const auto     fragShader = Util::VK::readSpirvShader(std::filesystem::path("window.frag.spv"));
    const auto     fragInfo   = Util::VK::shaderModuleCreateInfo(fragShader);
    VkShaderModule fragShaderModule;
    vkCreateShaderModule(m_window->device(), &fragInfo, nullptr, &fragShaderModule);

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
        },
    };

    const auto vertInput = VkPipelineVertexInputStateCreateInfo{
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

    const auto msState = VkPipelineMultisampleStateCreateInfo{
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
    const auto colorBlendAttachState = VkPipelineColorBlendAttachmentState{
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
        .pAttachments    = &colorBlendAttachState,
        .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    const auto layoutInfo = VkPipelineLayoutCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1,
        .pSetLayouts            = &m_descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = nullptr,
    };
    vkCreatePipelineLayout(m_window->device(), &layoutInfo, nullptr, &m_pipelineLayout);

    const auto colourFormat     = m_window->colorFormat();
    const auto renderCreateInfo = VkPipelineRenderingCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                   = nullptr,
        .viewMask                = 0,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &colourFormat,
        .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };
    const auto pipeInfo = VkGraphicsPipelineCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &renderCreateInfo,
        .flags               = 0,
        .stageCount          = 2,
        .pStages             = pipelineShaderStages.data(),
        .pVertexInputState   = &vertInput,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterState,
        .pMultisampleState   = &msState,
        .pDepthStencilState  = nullptr,
        .pColorBlendState    = &colorBlendState,
        .pDynamicState       = &dynamicState,
        .layout              = m_pipelineLayout,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
    };
    vkCreateGraphicsPipelines(m_window->device(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_pipeline);

    vkDestroyShaderModule(m_window->device(), vertShaderModule, nullptr);
    vkDestroyShaderModule(m_window->device(), fragShaderModule, nullptr);
}

auto VulkanRenderer::initSwapChainResources() -> void {
    const auto swapCount = static_cast<uint32_t>(m_window->swapChainImageCount());
    // descriptor sets
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_window->device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSets.clear();
    }

    const auto descPoolSize = VkDescriptorPoolSize{
        .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = swapCount,
    };
    const auto descPoolInfo = VkDescriptorPoolCreateInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .maxSets       = swapCount,
        .poolSizeCount = 1,
        .pPoolSizes    = &descPoolSize,
    };
    vkCreateDescriptorPool(m_window->device(), &descPoolInfo, nullptr, &m_descriptorPool);

    auto       layouts   = std::vector<VkDescriptorSetLayout>(swapCount, m_descriptorSetLayout);
    const auto allocInfo = VkDescriptorSetAllocateInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = nullptr,
        .descriptorPool     = m_descriptorPool,
        .descriptorSetCount = swapCount,
        .pSetLayouts        = layouts.data(),
    };
    m_descriptorSets.resize(swapCount);
    vkAllocateDescriptorSets(m_window->device(), &allocInfo, m_descriptorSets.data());
}

auto VulkanRenderer::releaseSwapChainResources() -> void {}

auto VulkanRenderer::releaseResources() -> void {
    m_rdpBackend->destroy();

    if (m_window->device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_window->device());
        vkDestroyPipeline(m_window->device(), m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_window->device(), m_pipelineLayout, nullptr);
        vkDestroyDescriptorPool(m_window->device(), m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_window->device(), m_descriptorSetLayout, nullptr);
        vkDestroySampler(m_window->device(), m_sampler, nullptr);
    }
}

auto VulkanRenderer::startNextFrame() -> void {
    const auto cmdBuf    = m_window->currentCommandBuffer();
    const auto swapImage = m_window->swapChainImage(m_window->currentSwapChainImageIndex());
    const auto swapView  = m_window->swapChainImageView(m_window->currentSwapChainImageIndex());
    const auto size      = m_window->swapChainImageSize();
    const auto dstExtent = VkExtent2D{
        .width  = static_cast<uint32_t>(size.width()),
        .height = static_cast<uint32_t>(size.height()),
    };

    const auto frame = m_rdpBackend->getRenderOutput();
    if (frame.m_image == VK_NULL_HANDLE || frame.m_extent.width == 0 || frame.m_extent.height == 0) {
        renderNothing(swapImage, swapView);
        {
            auto lock = std::lock_guard<std::mutex>(m_rdpBackend->queueMutex());
            m_window->frameReady();
        }
        m_window->requestUpdate();
        return;
    }

    const auto descriptorSet = m_descriptorSets[m_window->currentSwapChainImageIndex()];
    const auto imageInfo     = VkDescriptorImageInfo{
            .sampler     = m_sampler,
            .imageView   = frame.m_imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const auto writeDescSet = VkWriteDescriptorSet{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = nullptr,
        .dstSet           = descriptorSet,
        .dstBinding       = 0,
        .dstArrayElement  = 0,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &imageInfo,
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr,
    };
    vkUpdateDescriptorSets(m_window->device(), 1, &writeDescSet, 0, nullptr);

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
    vkCmdPipelineBarrier2(m_window->currentCommandBuffer(), &preDepInfo);

    const auto colourAttach = VkRenderingAttachmentInfo{
        .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext              = nullptr,
        .imageView          = swapView,
        .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode        = VK_RESOLVE_MODE_NONE,
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue         = VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
    };

    const auto renderInfo = VkRenderingInfo{
        .sType      = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext      = nullptr,
        .flags      = 0,
        .renderArea = VkRect2D{
            .offset = VkOffset2D{.x = 0, .y = 0},
            .extent = VkExtent2D{.width = static_cast<uint32_t>(m_window->swapChainImageSize().width()), .height = static_cast<uint32_t>(m_window->swapChainImageSize().height())},
        },
        .layerCount           = 1,
        .viewMask             = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colourAttach,
        .pDepthAttachment     = nullptr,
        .pStencilAttachment   = nullptr,
    };
    vkCmdBeginRendering(m_window->currentCommandBuffer(), &renderInfo);
    vkCmdBindPipeline(m_window->currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(
        m_window->currentCommandBuffer(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayout,
        0,
        1,
        &descriptorSet,
        0,
        nullptr);
    const auto viewport = VkViewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(m_window->swapChainImageSize().width()),
        .height   = static_cast<float>(m_window->swapChainImageSize().height()),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(m_window->currentCommandBuffer(), 0, 1, &viewport);
    const auto scissor = VkRect2D{
        .offset = VkOffset2D{.x = 0, .y = 0},
        .extent = VkExtent2D{.width = static_cast<uint32_t>(m_window->swapChainImageSize().width()), .height = static_cast<uint32_t>(m_window->swapChainImageSize().height())},
    };
    vkCmdSetScissor(m_window->currentCommandBuffer(), 0, 1, &scissor);
    vkCmdDraw(m_window->currentCommandBuffer(), 3, 1, 0, 0);
    vkCmdEndRendering(m_window->currentCommandBuffer());

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
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };
    const auto postDep = VkDependencyInfo{
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
    vkCmdPipelineBarrier2(m_window->currentCommandBuffer(), &postDep);

    {
        auto lock = std::lock_guard<std::mutex>(m_rdpBackend->queueMutex());
        m_window->frameReady();
    }
    m_window->requestUpdate();
}

auto VulkanRenderer::renderNothing(VkImage swapImage, VkImageView swapView) -> void {
    const auto clearBarrier = VkImageMemoryBarrier2{
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
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };
    const auto clearDep = VkDependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = 0,
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers    = nullptr,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &clearBarrier,
    };
    vkCmdPipelineBarrier2(m_window->currentCommandBuffer(), &clearDep);

    const auto clearColor = VkClearColorValue{.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
    const auto clearRange = VkImageSubresourceRange{
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    const auto renderAttachInfo = VkRenderingAttachmentInfo{
        .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext              = nullptr,
        .imageView          = swapView,
        .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode        = VK_RESOLVE_MODE_NONE,
        .resolveImageView   = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue         = VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    const auto renderInfo = VkRenderingInfo{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext                = nullptr,
        .flags                = 0,
        .renderArea           = VkRect2D{.offset = {0, 0},
                                         .extent = VkExtent2D{
                                             .width  = static_cast<uint32_t>(m_window->swapChainImageSize().width()),
                                             .height = static_cast<uint32_t>(m_window->swapChainImageSize().height())}},
        .layerCount           = 1,
        .viewMask             = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &renderAttachInfo,
        .pDepthAttachment     = nullptr,
        .pStencilAttachment   = nullptr,
    };
    vkCmdBeginRendering(m_window->currentCommandBuffer(), &renderInfo);
    vkCmdEndRendering(m_window->currentCommandBuffer());
    const auto presentBarrier = VkImageMemoryBarrier2{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = VK_PIPELINE_STAGE_2_CLEAR_BIT,
        .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask        = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask       = VK_ACCESS_2_NONE,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swapImage,
        .subresourceRange    = VkImageSubresourceRange{
               .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel   = 0,
               .levelCount     = 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
        },
    };
    const auto postClearDep = VkDependencyInfo{
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .dependencyFlags          = 0,
        .memoryBarrierCount       = 0,
        .pMemoryBarriers          = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers    = nullptr,
        .imageMemoryBarrierCount  = 1,
        .pImageMemoryBarriers     = &presentBarrier,
    };
    vkCmdPipelineBarrier2(m_window->currentCommandBuffer(), &postClearDep);
}
} // namespace GUI