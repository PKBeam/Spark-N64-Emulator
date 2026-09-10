module;
#include <atomic>
export module Gui;

extern "C++" { // Forward declarations for Qt types
struct QGuiApplication;
struct QTimer;
struct QVulkanWindow;
struct QVulkanInstance;
}

export namespace GUI {

class Application {
  public:
    Application(int argc, char* argv[], std::atomic<bool>& shouldTerminate);
    ~Application();

    auto run() -> int;
    auto exit() -> void;

    auto getVulkanInstance() const -> QVulkanInstance* {
        return m_vkInst;
    }

  private:
    QGuiApplication* m_app;
    QTimer*          m_timer;
    QVulkanInstance* m_vkInst;
};

class Window {
  public:
    Window(QVulkanInstance* vkInst);
    ~Window();
    auto show() -> void;

  private:
    QVulkanInstance* m_vkInst;
    QVulkanWindow*   m_vkWindow;
};

} // namespace GUI