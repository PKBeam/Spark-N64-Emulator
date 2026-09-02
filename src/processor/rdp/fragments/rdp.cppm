module;

#include <util/defines.hpp>

export module RDP:RDP;

import std;
import Interfaces;
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
    ::RDP::Control*               m_rdpControl;
    Interfaces::MipsInterface*    m_mipsInterface;
};

auto RDP::runCommand() -> void {
    const auto cmds = m_rdpControl->getCommands();
    if (cmds.empty()) {
        return;
    }
    for (const auto cmd : cmds) {
        const auto cmdType = (cmd >> 56) & 0x3F;
        std::println("Command: {}",
                     Util::enumName(static_cast<Command>(cmdType)).value_or(std::format("Unknown Command {:#018x}", cmd)));
    }
    std::println("Received {} commands from RDP", cmds.size());
    if (m_logger) {
        m_logger->flush();
    }
    std::terminate();
}
} // namespace RDP
