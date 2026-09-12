module;
#include <atomic>
#include <rdp_gfx_backend/gfxBackend.hpp>
export module Gui;

extern "C++" { // Forward declarations for Qt types
struct QGuiApplication;
struct QTimer;
struct QVulkanWindow;
struct QVulkanInstance;
}

export namespace GUI {

struct VkWindow;

class Window {
  public:
    Window(QVulkanInstance* vkInst, RDP::GfxBackend* rdpGfxBackend);
    ~Window();
    auto show() -> void;
    auto destroy() -> void;

  private:
    // owned by Application. don't delete in Window::~Window
    QVulkanInstance* m_vkInst;
    RDP::GfxBackend* m_rdpGfxBackend;
    VkWindow*        m_vkWindow;
};

class Application {
  public:
    Application(int& argc, char* argv[], RDP::GfxBackend* rdpGfxBackend, std::atomic<bool>& shouldTerminate);
    ~Application();

    auto run() -> int;
    auto quit() -> void;

  private:
    QGuiApplication* m_app;
    QTimer*          m_timer;
    QVulkanInstance* m_vkInst;
    RDP::GfxBackend* m_rdpGfxBackend;
    Window*          m_window;
};
} // namespace GUI