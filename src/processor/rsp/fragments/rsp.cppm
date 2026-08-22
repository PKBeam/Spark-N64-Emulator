module;

#include <util/defines.hpp>

export module RSP:RSP;

import std;
import Memory;
import RspControl;
import Util;

using Control = RSP::Control;

export namespace RSP {

class RSP {
  public:
    RSP(std::shared_ptr<Util::Logger> logger, Memory::Memory* memory, Control* control)
        : m_logger(logger), m_memory(memory), m_control(control) {};

    auto runInstruction() -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;

    Memory::Memory* m_memory;
    Control*        m_control;
};

auto RSP::runInstruction() -> void {
    if (m_control->getHalt()) {
        return;
    }
    // TODO

    constexpr auto imemBase = Memory::rangeOf(Memory::PhysSeg::RSP_IMEM).lower;

    auto pc = m_control->getPc() + imemBase;

    auto instBits = WITH_LOG_DISABLED(m_logger, m_memory->readPhysical<uint32_t>(pc));

    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", "RSP"},
            std::tuple{"pc", "0x{:08x}", static_cast<uint32_t>(pc)},
            std::tuple{"inst", "{:#08x}", instBits});
    }
    m_control->incrementPc();

    if (m_control->getSingleStep()) {
        m_control->setHalt(true);
    }
}

} // namespace RSP
