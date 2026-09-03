module;

#include <util/defines.hpp>

export module RDP:RDP;

import std;
import Interfaces;
import InterfaceTypes;
import ISA;
import RdpControl;
import Util;

import :Commands;

export namespace RDP {

class RDP {
  public:
    RDP(std::shared_ptr<Util::Logger> logger,
        ::RDP::Control*               rdpControl,
        Interfaces::MipsInterface*    mipsInterface)
        : m_logger(logger), m_rdpControl(rdpControl), m_mipsInterface(mipsInterface) {};

    auto runCommand() -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    ::RDP::Control*               m_rdpControl{};
    Interfaces::MipsInterface*    m_mipsInterface{};
};

auto RDP::runCommand() -> void {
    const auto cmds = m_rdpControl->getCommands();
    if (cmds.empty()) {
        return;
    }
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sev::INFO, Sys::RDP>("Received {} commands", cmds.size());
    }
    for (const auto cmd : cmds) {
        const auto cmdType = getCommand(cmd);
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::HIGH, Sys::RDP>(
                std::tuple{"command", "{}", cmdType});
        }
        switch (cmdType) {
            case Command::SYNC_FULL: {
                m_mipsInterface->setInterrupt<^^Interfaces::MI_INTERRUPT::dp>(true);
                break;
            }
            default: break;
        }
    }
    static int numFrames = 0;
    if (numFrames++ == 10) { // terminate after 10 frames
        IF_LOG_ENABLED(m_logger) {
            m_logger->flush();
        }
        std::terminate();
    }
}
} // namespace RDP
