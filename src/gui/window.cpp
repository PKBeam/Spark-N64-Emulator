module;
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QTimer>
#include <QVulkanWindow>
#include <QVulkanFunctions>
#include <QFile>

#include "vkUtil.hpp"

module Gui;

constexpr auto vertexData = std::array<float, 15>{
    // Y up, front = CCW
    // clang-format off
     0.0f,   0.5f,   1.0f, 0.0f, 0.0f,
    -0.5f,  -0.5f,   0.0f, 1.0f, 0.0f,
     0.5f,  -0.5f,   0.0f, 0.0f, 1.0f
    // clang-format on
};

namespace GUI {

class VulkanRenderer : public QVulkanWindowRenderer {
  public:
    VulkanRenderer(QVulkanWindow* w) : m_window(w) {}

    auto initResources() -> void override;
    auto initSwapChainResources() -> void override;
    auto releaseSwapChainResources() -> void override;
    auto releaseResources() -> void override;
    auto startNextFrame() -> void override;

  private:
    auto createShader(const QString& name) -> VkShaderModule;

    QVulkanWindow*          m_window;
    QVulkanDeviceFunctions* m_devFuncs;

    VkDeviceMemory                                                                m_bufMem = VK_NULL_HANDLE;
    VkBuffer                                                                      m_buf    = VK_NULL_HANDLE;
    std::array<VkDescriptorBufferInfo, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT> m_uniformBufInfo;

    VkDescriptorPool                                                       m_descPool      = VK_NULL_HANDLE;
    VkDescriptorSetLayout                                                  m_descSetLayout = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT> m_descSet;

    VkPipelineCache  m_pipelineCache  = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;

    QMatrix4x4 m_proj;
    float      m_rotation = 0.0f;
};

auto VulkanRenderer::createShader(const QString& name) -> VkShaderModule {
    auto file = QFile(name);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read shader %s", qPrintable(name));
        return VK_NULL_HANDLE;
    }
    auto blob = file.readAll();
    file.close();

    const auto shaderInfo = VkShaderModuleCreateInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .codeSize = static_cast<size_t>(blob.size()),
        .pCode    = reinterpret_cast<const uint32_t*>(blob.constData()),
    };

    auto shaderModule = VkShaderModule{};

    auto err = m_devFuncs->vkCreateShaderModule(m_window->device(), &shaderInfo, nullptr, &shaderModule);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create shader module: %d", err);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

auto VulkanRenderer::initResources() -> void {
    VkDevice dev = m_window->device();
    m_devFuncs   = m_window->vulkanInstance()->deviceFunctions(dev);

    // Prepare the vertex and uniform data. The vertex data will never
    // change so one buffer is sufficient regardless of the value of
    // QVulkanWindow::CONCURRENT_FRAME_COUNT. Uniform data is changing per
    // frame however so active frames have to have a dedicated copy.

    // Use just one memory allocation and one buffer. We will then specify the
    // appropriate offsets for uniform buffers in the VkDescriptorBufferInfo.
    // Have to watch out for
    // VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment, though.

    // The uniform buffer is not strictly required in this example, we could
    // have used push constants as well since our single matrix (64 bytes) fits
    // into the spec mandated minimum limit of 128 bytes. However, once that
    // limit is not sufficient, the per-frame buffers, as shown below, will
    // become necessary.

    const auto  concurrentFrameCount = static_cast<uint32_t>(m_window->concurrentFrameCount());
    const auto& pdevLimits           = m_window->physicalDeviceProperties()->limits;
    const auto  uniAlign             = pdevLimits.minUniformBufferOffsetAlignment;
    qDebug("uniform buffer offset alignment is %u", (uint)uniAlign);

    const auto vertexAllocSize  = VK::aligned(sizeof(vertexData), uniAlign);
    const auto uniformAllocSize = VK::aligned(16 * sizeof(float), uniAlign);

    const auto bufInfo = VkBufferCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .size                  = vertexAllocSize + concurrentFrameCount * uniformAllocSize,
        .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
    };

    VK::tryFunc(m_devFuncs->vkCreateBuffer(dev, &bufInfo, nullptr, &m_buf), "create buffer");

    VkMemoryRequirements memReq;
    m_devFuncs->vkGetBufferMemoryRequirements(dev, m_buf, &memReq);

    const auto memAllocInfo = VkMemoryAllocateInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = nullptr,
        .allocationSize  = memReq.size,
        .memoryTypeIndex = m_window->hostVisibleMemoryIndex()};

    VK::tryFunc(m_devFuncs->vkAllocateMemory(dev, &memAllocInfo, nullptr, &m_bufMem), "allocate memory");

    VK::tryFunc(m_devFuncs->vkBindBufferMemory(dev, m_buf, m_bufMem, 0), "bind buffer memory");

    std::byte* memory;
    VK::tryFunc(m_devFuncs->vkMapMemory(dev, m_bufMem, 0, memReq.size, 0, reinterpret_cast<void**>(&memory)), "map memory");
    memcpy(memory, vertexData.data(), sizeof(vertexData));

    const auto ident = QMatrix4x4();
    for (auto i : std::views::iota(0uz, concurrentFrameCount)) {
        const auto offset = VkDeviceSize(vertexAllocSize + i * uniformAllocSize);
        memcpy(memory + offset, ident.data(), 16 * sizeof(float));
        m_uniformBufInfo[i] = VkDescriptorBufferInfo{
            .buffer = m_buf,
            .offset = offset,
            .range  = uniformAllocSize,
        };
    }
    m_devFuncs->vkUnmapMemory(dev, m_bufMem);

    const auto vertexBindingDesc = VkVertexInputBindingDescription{
        .binding   = 0,
        .stride    = 5 * sizeof(float),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

    const auto positionAttrDesc = VkVertexInputAttributeDescription{
        .location = 0,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32_SFLOAT,
        .offset   = 0};
    const auto colourAttrDesc = VkVertexInputAttributeDescription{
        .location = 1,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = 2 * sizeof(float)};
    const auto vertexAttrDesc = std::array<VkVertexInputAttributeDescription, 2>{positionAttrDesc, colourAttrDesc};

    const auto vertexInputInfo = VkPipelineVertexInputStateCreateInfo{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &vertexBindingDesc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions    = vertexAttrDesc.data(),
    };

    // Set up descriptor set and its layout.
    const auto descPoolSizes = VkDescriptorPoolSize{
        .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = concurrentFrameCount};
    const auto descPoolInfo = VkDescriptorPoolCreateInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .maxSets       = concurrentFrameCount,
        .poolSizeCount = 1,
        .pPoolSizes    = &descPoolSizes,
    };
    VK::tryFunc(m_devFuncs->vkCreateDescriptorPool(dev, &descPoolInfo, nullptr, &m_descPool), "create descriptor pool");

    const auto layoutBinding = VkDescriptorSetLayoutBinding{
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr};
    const auto descLayoutInfo = VkDescriptorSetLayoutCreateInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = 1,
        .pBindings    = &layoutBinding};
    VK::tryFunc(m_devFuncs->vkCreateDescriptorSetLayout(dev, &descLayoutInfo, nullptr, &m_descSetLayout), "create descriptor set layout");

    for (auto i : std::views::iota(0uz, concurrentFrameCount)) {
        const auto descSetAllocInfo = VkDescriptorSetAllocateInfo{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = m_descPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &m_descSetLayout};

        VK::tryFunc(m_devFuncs->vkAllocateDescriptorSets(dev, &descSetAllocInfo, &m_descSet[i]), "allocate descriptor set");

        const auto descWrite = VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = m_descSet[i],
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &m_uniformBufInfo[i],
            .pTexelBufferView = nullptr};
        m_devFuncs->vkUpdateDescriptorSets(dev, 1, &descWrite, 0, nullptr);
    }

    const auto pipelineCacheInfo = VkPipelineCacheCreateInfo{
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .initialDataSize = 0,
        .pInitialData    = nullptr,
    };
    VK::tryFunc(m_devFuncs->vkCreatePipelineCache(dev, &pipelineCacheInfo, nullptr, &m_pipelineCache), "create pipeline cache");

    const auto pipelineLayoutInfo = VkPipelineLayoutCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1,
        .pSetLayouts            = &m_descSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = nullptr,
    };
    VK::tryFunc(m_devFuncs->vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout), "create pipeline layout");

    // Shaders
    const auto vertShaderModule = createShader(QStringLiteral("./color_vert.spv"));
    const auto fragShaderModule = createShader(QStringLiteral("./color_frag.spv"));

    // Graphics pipeline
    auto pipelineInfo = VkGraphicsPipelineCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stageCount          = 2,
        .pStages             = nullptr, // will be set later
        .pVertexInputState   = nullptr, // will be set later
        .pInputAssemblyState = nullptr, // will be set later
        .pTessellationState  = nullptr,
        .pViewportState      = nullptr, // will be set later
        .pRasterizationState = nullptr,
        .pMultisampleState   = nullptr,
        .pDepthStencilState  = nullptr,
        .pColorBlendState    = nullptr,
        .pDynamicState       = nullptr,
        .layout              = m_pipelineLayout,
        .renderPass          = m_window->defaultRenderPass(),
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
    };

    const auto shaderStages = std::array<VkPipelineShaderStageCreateInfo, 2>{
        VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_VERTEX_BIT,
            .module              = vertShaderModule,
            .pName               = "main",
            .pSpecializationInfo = nullptr},
        VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module              = fragShaderModule,
            .pName               = "main",
            .pSpecializationInfo = nullptr}};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages    = shaderStages.data();

    pipelineInfo.pVertexInputState = &vertexInputInfo;

    const auto ia = VkPipelineInputAssemblyStateCreateInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    pipelineInfo.pInputAssemblyState = &ia;

    // The viewport and scissor will be set dynamically via vkCmdSetViewport/Scissor.
    // This way the pipeline does not need to be touched when resizing the window.
    auto vp = VkPipelineViewportStateCreateInfo{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = nullptr, // Dynamic viewport will be set via vkCmdSetViewport
        .scissorCount  = 1,
        .pScissors     = nullptr, // Dynamic scissor will be set via vkCmdSetScissor
    };
    pipelineInfo.pViewportState = &vp;

    const auto rs = VkPipelineRasterizationStateCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_NONE, // we want the back face as well
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f,
        .depthBiasSlopeFactor    = 0.0f,
        .lineWidth               = 1.0f,
    };
    pipelineInfo.pRasterizationState = &rs;

    const auto ms = VkPipelineMultisampleStateCreateInfo{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .rasterizationSamples  = m_window->sampleCountFlagBits(),
        .sampleShadingEnable   = VK_FALSE,
        .minSampleShading      = 0.0f,
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
    };
    pipelineInfo.pMultisampleState = &ms;

    const auto ds = VkPipelineDepthStencilStateCreateInfo{
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
    pipelineInfo.pDepthStencilState = &ds;

    const auto att = VkPipelineColorBlendAttachmentState{
        .blendEnable         = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = 0xF,
    };
    const auto cb = VkPipelineColorBlendStateCreateInfo{
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &att,
        .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
    };
    pipelineInfo.pColorBlendState = &cb;

    const auto dynEnable = std::array<VkDynamicState, 2>{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const auto dyn       = VkPipelineDynamicStateCreateInfo{
              .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
              .pNext             = nullptr,
              .flags             = 0,
              .dynamicStateCount = static_cast<uint32_t>(dynEnable.size()),
              .pDynamicStates    = dynEnable.data(),
    };
    pipelineInfo.pDynamicState = &dyn;

    pipelineInfo.layout     = m_pipelineLayout;
    pipelineInfo.renderPass = m_window->defaultRenderPass();

    VK::tryFunc(m_devFuncs->vkCreateGraphicsPipelines(dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline), "create graphics pipeline");

    if (vertShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, vertShaderModule, nullptr);
    if (fragShaderModule)
        m_devFuncs->vkDestroyShaderModule(dev, fragShaderModule, nullptr);
}

auto VulkanRenderer::initSwapChainResources() -> void {
    qDebug("initSwapChainResources");
    m_proj        = m_window->clipCorrectionMatrix(); // adjust for Vulkan-OpenGL clip space differences
    const auto sz = m_window->swapChainImageSize();
    m_proj.perspective(45.0f, sz.width() / (float)sz.height(), 0.01f, 100.0f);
    m_proj.translate(0, 0, -4);
}

auto VulkanRenderer::releaseSwapChainResources() -> void {
    qDebug("releaseSwapChainResources");
}

auto VulkanRenderer::releaseResources() -> void {
    qDebug("releaseResources");
    auto dev = m_window->device();

    if (m_pipeline) {
        m_devFuncs->vkDestroyPipeline(dev, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout) {
        m_devFuncs->vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_pipelineCache) {
        m_devFuncs->vkDestroyPipelineCache(dev, m_pipelineCache, nullptr);
        m_pipelineCache = VK_NULL_HANDLE;
    }

    if (m_descSetLayout) {
        m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_descSetLayout, nullptr);
        m_descSetLayout = VK_NULL_HANDLE;
    }

    if (m_descPool) {
        m_devFuncs->vkDestroyDescriptorPool(dev, m_descPool, nullptr);
        m_descPool = VK_NULL_HANDLE;
    }

    if (m_buf) {
        m_devFuncs->vkDestroyBuffer(dev, m_buf, nullptr);
        m_buf = VK_NULL_HANDLE;
    }

    if (m_bufMem) {
        m_devFuncs->vkFreeMemory(dev, m_bufMem, nullptr);
        m_bufMem = VK_NULL_HANDLE;
    }
}

auto VulkanRenderer::startNextFrame() -> void {
    auto       dev = m_window->device();
    auto       cb  = m_window->currentCommandBuffer();
    const auto sz  = m_window->swapChainImageSize();

    auto clearColor  = VkClearColorValue{{0, 0, 0, 1}};
    auto clearDS     = VkClearDepthStencilValue{1, 0};
    auto clearValues = std::array<VkClearValue, 3>{
        VkClearValue{.color = clearColor},
        VkClearValue{.depthStencil = clearDS},
        VkClearValue{.color = clearColor},
    };

    const auto rpBeginInfo = VkRenderPassBeginInfo{
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext       = nullptr,
        .renderPass  = m_window->defaultRenderPass(),
        .framebuffer = m_window->currentFramebuffer(),
        .renderArea  = {
             .offset = {0, 0},
             .extent = {static_cast<uint32_t>(sz.width()), static_cast<uint32_t>(sz.height())},
        },
        .clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3u : 2u,
        .pClearValues    = clearValues.data(),
    };

    const auto cmdBuf = m_window->currentCommandBuffer();
    m_devFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    quint8* p;
    VK::tryFunc(m_devFuncs->vkMapMemory(dev, m_bufMem, m_uniformBufInfo[m_window->currentFrame()].offset, 16 * sizeof(float), 0, reinterpret_cast<void**>(&p)));
    auto m = m_proj;
    m.rotate(m_rotation, 0, 1, 0);
    memcpy(p, m.constData(), 16 * sizeof(float));
    m_devFuncs->vkUnmapMemory(dev, m_bufMem);

    // Not exactly a real animation system, just advance on every frame for now.
    m_rotation += 1.0f;

    m_devFuncs->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    m_devFuncs->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet[m_window->currentFrame()], 0, nullptr);
    VkDeviceSize vbOffset = 0;
    m_devFuncs->vkCmdBindVertexBuffers(cb, 0, 1, &m_buf, &vbOffset);

    const auto viewport = VkViewport{
        .x        = 0,
        .y        = 0,
        .width    = static_cast<float>(sz.width()),
        .height   = static_cast<float>(sz.height()),
        .minDepth = 0,
        .maxDepth = 1,
    };
    m_devFuncs->vkCmdSetViewport(cb, 0, 1, &viewport);

    const auto scissor = VkRect2D{
        .offset = {0, 0},
        .extent = {static_cast<uint32_t>(sz.width()), static_cast<uint32_t>(sz.height())},
    };
    m_devFuncs->vkCmdSetScissor(cb, 0, 1, &scissor);

    m_devFuncs->vkCmdDraw(cb, 3, 1, 0, 0);

    m_devFuncs->vkCmdEndRenderPass(cmdBuf);

    m_window->frameReady();
    m_window->requestUpdate(); // render continuously, throttled by the presentation rate
}

class VkWindow : public QVulkanWindow {
  public:
    auto createRenderer() -> QVulkanWindowRenderer* override {
        return new VulkanRenderer(this);
    }
};

Application::Application(int argc, char* argv[], std::atomic<bool>& shouldTerminate) : m_app(new QGuiApplication(argc, argv)),
                                                                                       m_timer(new QTimer()),
                                                                                       m_vkInst(new QVulkanInstance()) {
    m_vkInst->setLayers(QByteArrayList()
                        << "VK_LAYER_KHRONOS_validation"
                        << "VK_LAYER_GOOGLE_threading"
                        << "VK_LAYER_LUNARG_parameter_validation"
                        << "VK_LAYER_LUNARG_object_tracker"
                        << "VK_LAYER_LUNARG_core_validation"
                        << "VK_LAYER_LUNARG_image"
                        << "VK_LAYER_LUNARG_swapchain"
                        << "VK_LAYER_GOOGLE_unique_objects");
    if (!m_vkInst->create())
        qFatal("Failed to create Vulkan instance: %d", m_vkInst->errorCode());

    m_app->connect(m_timer, &QTimer::timeout, m_app, [this, &shouldTerminate]() {
        if (shouldTerminate) {
            m_app->quit();
        }
    });
    m_timer->start(1000);
}

Application::~Application() {
    // TODO stop GPU rendering here
    m_vkInst->destroy();
    delete m_vkInst;

    m_timer->stop();
    delete m_timer;

    delete m_app;
}

auto Application::run() -> int {
    return m_app->exec();
}

Window::Window(QVulkanInstance* vkInst)
    : m_vkInst(vkInst), m_vkWindow(new VkWindow()) {
}

Window::~Window() {
    delete m_vkWindow;
}

auto Window::show() -> void {
    m_vkWindow->setVulkanInstance(m_vkInst);
    m_vkWindow->resize(320 * 4, 240 * 4);
    m_vkWindow->show();
}

} // namespace GUI