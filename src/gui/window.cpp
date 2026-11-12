module;
// Must be included before any Qt Vulkan headers, which define VK_NO_PROTOTYPES and would otherwise hide the C API declarations.
#include <vulkan/vulkan.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QEvent>
#include <QExposeEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QVulkanInstance>
#include <QWindow>
#include <QFile>

#include "vkRenderWindow.hpp"

#include <cstddef>
#include <exception>
#include <iostream>

#ifdef __APPLE__
#include <objc/message.h>
#include <objc/runtime.h>
#endif

module Gui;

#ifdef __APPLE__
void force_window_onto_active_screen(QWindow* qwindow) {
    if (!qwindow) return;
    id nsView = (id)qwindow->winId();
    if (!nsView) return;

    id nsWin = ((id (*)(id, SEL))objc_msgSend)(nsView, sel_registerName("window"));
    if (nsWin) {
        // 1. Allow window to join all active Spaces and Stage Manager
        // NSWindowCollectionBehaviorCanJoinAllSpaces (1) | NSWindowCollectionBehaviorStationary (16)
        ((void (*)(id, SEL, std::size_t))objc_msgSend)(nsWin, sel_registerName("setCollectionBehavior:"), 17);

        // 2. Force Quartz to order the window front REGARDLESS of the app's internal focus state
        ((void (*)(id, SEL))objc_msgSend)(nsWin, sel_registerName("orderFrontRegardless"));

        // 3. Elevate app to a foreground GUI process (gives it a Dock icon and focus rights)
        id nsAppClass = (id)objc_getClass("NSApplication");
        id nsApp      = ((id (*)(id, SEL))objc_msgSend)(nsAppClass, sel_registerName("sharedApplication"));
        if (nsApp) {
            ((void (*)(id, SEL, long long))objc_msgSend)(nsApp, sel_registerName("setActivationPolicy:"), 0);
            ((void (*)(id, SEL, bool))objc_msgSend)(nsApp, sel_registerName("activateIgnoringOtherApps:"), true);
        }
    }
}
#endif

namespace GUI {

class VkWindow : public QWindow {
  public:
    auto renderFrame() -> void {
        if (!m_renderer || !isExposed()) {
            return;
        }
        try {
            m_renderer->render();
        } catch (const std::exception& error) {
            qFatal("Vulkan rendering failed: %s", error.what());
        }
    }

    VulkanRenderer* m_renderer = nullptr;

  protected:
    auto event(QEvent* event) -> bool override {
        if (event->type() == QEvent::UpdateRequest) {
            renderFrame();
        }
        return QWindow::event(event);
    }

    auto exposeEvent(QExposeEvent* event) -> void override {
        QWindow::exposeEvent(event);
        if (isExposed()) {
            // Let Qt finish the expose callback before MoltenVK acquires a drawable.
            requestUpdate();
        }
    }

    auto resizeEvent(QResizeEvent* event) -> void override {
        QWindow::resizeEvent(event);
        if (m_renderer) {
            m_renderer->markSwapChainDirty();
            if (isExposed()) {
                requestUpdate();
            }
        }
    }
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
    m_app->quit();
}

Window::Window(QVulkanInstance* vkInst, RDP::GfxBackend* rdpGfxBackend)
    : m_vkInst(vkInst),
      m_rdpGfxBackend(rdpGfxBackend),
      m_vkWindow(new VkWindow()),
      m_frameCallbackWindow(new QPointer<VkWindow>(m_vkWindow)) {

    auto vkBackend = dynamic_cast<RDP::VulkanBackend*>(m_rdpGfxBackend);
    if (!vkBackend) {
        qFatal("Failed to cast GfxBackend to VulkanBackend");
    }
    m_vkWindow->setSurfaceType(QSurface::VulkanSurface);
    m_vkWindow->QWindow::setFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                  Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
    m_vkWindow->setTitle("spark");
    m_vkWindow->setMinimumSize({320, 240});
    m_vkWindow->m_renderer = new VulkanRenderer(m_vkWindow, m_vkInst, vkBackend);
    static_cast<RDP::VulkanBackend*>(m_rdpGfxBackend)->setFrameCompleteCallback(requestWindowUpdate, m_frameCallbackWindow);
}

Window::~Window() {
    static_cast<RDP::VulkanBackend*>(m_rdpGfxBackend)->setFrameCompleteCallback(nullptr, nullptr);
    delete static_cast<QPointer<VkWindow>*>(m_frameCallbackWindow);
    delete m_vkWindow->m_renderer;
    m_vkWindow->setVulkanInstance(nullptr);
    delete m_vkWindow;
}

auto Window::show() -> void {
    m_vkWindow->setVulkanInstance(m_vkInst);
    m_vkWindow->resize(320 * 4, 240 * 4);
    m_vkWindow->show();
    m_vkWindow->raise();
    m_vkWindow->requestActivate();
    // QTimer::singleShot(2000, m_vkWindow, dump_nsapp_windows_gcc);
}

auto Window::destroy() -> void {
    if (m_vkWindow->m_renderer) {
        try {
            m_vkWindow->m_renderer->shutdown();
        } catch (const std::exception& error) {
            qFatal("Vulkan shutdown failed: %s", error.what());
        }
    }
    m_vkWindow->close();
    m_vkWindow->destroy();
}

} // namespace GUI

//"env": {
//    "QT_LOGGING_RULES": "qt.vulkan*=true;qt.qpa.window=true",
//    "QT_DEBUG_PLUGINS": "1",
//    "MVK_CONFIG_LOG_LEVEL": "3"
//}
