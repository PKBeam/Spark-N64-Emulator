module;
// Must be included before any Qt Vulkan headers, which define VK_NO_PROTOTYPES and would otherwise hide the C API declarations.
#include <vulkan/vulkan.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QVulkanWindow>
#include <QVulkanDeviceFunctions>
#include <QVulkanFunctions>
#include <QVulkanInstance>
#include <QFile>

#include "vkRenderWindow.hpp"

module Gui;

namespace GUI {

class VkWindow : public QVulkanWindow {
  public:
    auto createRenderer() -> QVulkanWindowRenderer* override;

    RDP::GfxBackend* m_rdpGfxBackend;
};

namespace {
auto requestWindowUpdate(void* userData) -> void {
    const auto window = *static_cast<QPointer<VkWindow>*>(userData);
    if (!window) {
        return;
    }
    QMetaObject::invokeMethod(
        window.data(),
        [window]() {
            if (window) {
                window->requestUpdate();
            }
        },
        Qt::QueuedConnection);
}
} // namespace

auto VkWindow::createRenderer() -> QVulkanWindowRenderer* {
    auto vkBackend = dynamic_cast<RDP::VulkanBackend*>(m_rdpGfxBackend);
    if (!vkBackend) {
        qFatal("Failed to cast GfxBackend to VulkanBackend");
    }
    auto renderer = new VulkanRenderer(this, vkBackend);
    return renderer;
}

Application::Application(int& argc, char* argv[], RDP::GfxBackend* rdpGfxBackend, std::atomic<bool>& shouldTerminate)
    : m_app(new QGuiApplication(argc, argv)),
      m_timer(new QTimer()),
      m_vkInst(new QVulkanInstance()),
      m_rdpGfxBackend(rdpGfxBackend),
      m_window(new GUI::Window(m_vkInst, m_rdpGfxBackend)) {
#if defined(ENABLE_VK_VALIDATION)
    m_vkInst->setLayers(QByteArrayList() << "VK_LAYER_KHRONOS_validation");
#else
    m_vkInst->setLayers({});
#endif
    m_vkInst->setApiVersion(QVersionNumber(1, 4));
    m_vkInst->setExtensions(QByteArrayList() << "VK_EXT_swapchain_colorspace");
    if (!m_vkInst->create())
        qFatal("Failed to create Vulkan instance: %d", m_vkInst->errorCode());
    m_app->connect(m_timer, &QTimer::timeout, m_app, [this, &shouldTerminate]() {
        if (shouldTerminate) {
            quit();
        }
    });
    m_timer->start(1000);
}

Application::~Application() {
    delete m_window;
    delete m_vkInst;
    delete m_timer;
    delete m_app;
}

auto Application::run() -> int {
    m_window->show();
    return m_app->exec();
}

auto Application::quit() -> void {
    m_timer->stop();
    m_window->destroy();
    m_vkInst->destroy();
    m_app->quit();
}

Window::Window(QVulkanInstance* vkInst, RDP::GfxBackend* rdpGfxBackend)
    : m_vkInst(vkInst),
      m_rdpGfxBackend(rdpGfxBackend),
      m_vkWindow(new VkWindow()),
      m_frameCallbackWindow(new QPointer<VkWindow>(m_vkWindow)) {

    m_vkWindow->QWindow::setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                  Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    m_vkWindow->setTitle("spark");
    m_vkWindow->setMinimumSize({320, 240});
    m_vkWindow->setDeviceExtensions(QByteArrayList()
                                    << VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);

    constinit static auto features14 = VkPhysicalDeviceVulkan14Features{};
    features14.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    features14.pushDescriptor        = VK_TRUE;

    constinit static auto features13 = VkPhysicalDeviceVulkan13Features{};
    features13.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering      = VK_TRUE;
    features13.synchronization2      = VK_TRUE;

    constinit static auto features12                         = VkPhysicalDeviceVulkan12Features{};
    features12.sType                                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.shaderInt8                                    = VK_TRUE;
    features12.uniformAndStorageBuffer8BitAccess             = VK_TRUE;
    features12.storagePushConstant8                          = VK_TRUE;
    features12.scalarBlockLayout                             = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE;
    features12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;

    constinit static auto extendedDynamicState2Features = VkPhysicalDeviceExtendedDynamicState2FeaturesEXT{};
    extendedDynamicState2Features.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
    extendedDynamicState2Features.extendedDynamicState2 = VK_TRUE;

    m_vkWindow->setEnabledFeaturesModifier([this](VkPhysicalDeviceFeatures2& features2) {
        features2.pNext  = &features12;
        features12.pNext = &features13;
        features13.pNext = &features14;
        features14.pNext = &extendedDynamicState2Features;
    });
    m_vkWindow->m_rdpGfxBackend = m_rdpGfxBackend;
    static_cast<RDP::VulkanBackend*>(m_rdpGfxBackend)->setFrameCompleteCallback(requestWindowUpdate, m_frameCallbackWindow);
}

Window::~Window() {
    static_cast<RDP::VulkanBackend*>(m_rdpGfxBackend)->setFrameCompleteCallback(nullptr, nullptr);
    delete static_cast<QPointer<VkWindow>*>(m_frameCallbackWindow);
    m_vkWindow->setVulkanInstance(nullptr);
    delete m_vkWindow;
}

auto Window::show() -> void {
    m_vkWindow->setVulkanInstance(m_vkInst);
    m_vkWindow->resize(320 * 4, 240 * 4);
    m_vkWindow->show();
}

auto Window::destroy() -> void {
    m_vkWindow->close();
}

} // namespace GUI