module;
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
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
    : m_vkInst(vkInst), m_rdpGfxBackend(rdpGfxBackend), m_vkWindow(new VkWindow()) {

    m_vkWindow->QWindow::setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                  Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    m_vkWindow->setTitle("spark");
    m_vkWindow->setMinimumSize({320, 240});

    constinit static auto features13 = VkPhysicalDeviceVulkan13Features{};
    features13.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering      = VK_TRUE;
    features13.synchronization2      = VK_TRUE;

    constinit static auto scalarBlockLayoutFeatures = VkPhysicalDeviceScalarBlockLayoutFeatures{};
    scalarBlockLayoutFeatures.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
    scalarBlockLayoutFeatures.scalarBlockLayout     = VK_TRUE;

    constinit static auto storage8BitFeatures             = VkPhysicalDevice8BitStorageFeatures{};
    storage8BitFeatures.sType                             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
    storage8BitFeatures.uniformAndStorageBuffer8BitAccess = VK_TRUE;

    constinit static auto extendedDynamicState2Features = VkPhysicalDeviceExtendedDynamicState2FeaturesEXT{};
    extendedDynamicState2Features.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
    extendedDynamicState2Features.extendedDynamicState2 = VK_TRUE;

    m_vkWindow->setEnabledFeaturesModifier([this](VkPhysicalDeviceFeatures2& features2) {
        scalarBlockLayoutFeatures.pNext     = features2.pNext;
        storage8BitFeatures.pNext           = &scalarBlockLayoutFeatures;
        extendedDynamicState2Features.pNext = &storage8BitFeatures;
        features13.pNext                    = &extendedDynamicState2Features;
        features2.pNext                     = &features13;
    });
    m_vkWindow->m_rdpGfxBackend = m_rdpGfxBackend;
}

Window::~Window() {
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