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

Application::Application(int argc, char* argv[], RDP::GfxBackend* rdpGfxBackend, std::atomic<bool>& shouldTerminate)
    : m_app(new QGuiApplication(argc, argv)),
      m_timer(new QTimer()),
      m_vkInst(new QVulkanInstance()),
      m_rdpGfxBackend(rdpGfxBackend),
      m_window(new GUI::Window(m_vkInst, m_rdpGfxBackend)) {
    m_vkInst->setLayers(QByteArrayList()
                        << "VK_LAYER_KHRONOS_validation"
                        << "VK_LAYER_GOOGLE_threading"
                        << "VK_LAYER_LUNARG_parameter_validation"
                        << "VK_LAYER_LUNARG_object_tracker"
                        << "VK_LAYER_LUNARG_core_validation"
                        << "VK_LAYER_LUNARG_image"
                        << "VK_LAYER_LUNARG_swapchain"
                        << "VK_LAYER_GOOGLE_unique_objects");
    m_vkInst->setApiVersion(QVersionNumber(1, 4));
    m_vkInst->setExtensions(QByteArrayList()
                            << "VK_EXT_swapchain_colorspace"
                            << "VK_EXT_conservative_rasterization");
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

    constinit static auto features13 = VkPhysicalDeviceVulkan13Features{};
    features13.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering      = VK_TRUE;
    features13.synchronization2      = VK_TRUE;

    m_vkWindow->setEnabledFeaturesModifier([this](VkPhysicalDeviceFeatures2& features2) {
        features13.pNext = features2.pNext;
        features2.pNext  = &features13;
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