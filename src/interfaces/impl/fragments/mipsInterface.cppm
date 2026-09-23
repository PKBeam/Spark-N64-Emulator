module;
#include <util/defines.hpp>
export module Interfaces:MipsInterface;

import std;
import CP0;
import ISA;
import Util;

import :Interface;
import InterfaceTypes;

namespace Interfaces {

export class MipsInterface : public Interface {
  public:
    MipsInterface(std::shared_ptr<Util::Logger> logger, CP0::CP0* cp0) : m_logger(logger), m_cp0(cp0) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

    template <std::meta::info IntrField>
    auto setInterrupt(bool enable) -> void;

    template <std::meta::info IntrField>
    auto getInterrupt() const -> bool;

  private:
    auto updateInterrupt() -> void;

    std::shared_ptr<Util::Logger> m_logger;
    CP0::CP0*                     m_cp0{};

    std::atomic<MI_MODE>      m_mode{};
    std::atomic<MI_INTERRUPT> m_interrupt{};
    std::atomic<MI_MASK>      m_mask{};
};

auto MipsInterface::updateInterrupt() -> void {
    auto cause = m_cp0->readReg<ISA::CP0_REG::CAUSE>();

    const auto interrupts = std::bit_cast<uint32_t>(m_interrupt);
    const auto mask       = std::bit_cast<uint32_t>(m_mask);
    if (interrupts & mask) {
        cause.ip |= (1 << 2); // set IP2
        cause.exc = 0;
    } else {
        cause.ip &= ~(1 << 2); // clear IP2
    }
    m_cp0->writeReg(cause);
    m_cp0->updateInterrupt();
}

template <std::meta::info IntrField>
auto MipsInterface::setInterrupt(bool enable) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sev::INFO, Sys::MI>("{} interrupt {}",
                                                       enable ? "Set" : "Clear",
                                                       std::meta::identifier_of(IntrField));
    }
    auto newInterrupt          = m_interrupt.load();
    newInterrupt.[:IntrField:] = enable ? 1 : 0;
    m_interrupt.store(newInterrupt);
    updateInterrupt();
}

template <std::meta::info IntrField>
auto MipsInterface::getInterrupt() const -> bool {
    return m_interrupt.load().[:IntrField:] != 0;
}

auto MipsInterface::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    MI_REG_ADDR::BASE <= addr && addr <= MI_REG_ADDR::END);

    addr = MI_REG_ADDR::BASE + (addr & 0xF);

    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case MI_REG_ADDR::MI_MODE: return std::bit_cast<uint32_t>(m_mode);
            case MI_REG_ADDR::MI_VERSION: return std::bit_cast<uint32_t>(MI_VERSION{});
            case MI_REG_ADDR::MI_INTERRUPT: return std::bit_cast<uint32_t>(m_interrupt);
            case MI_REG_ADDR::MI_MASK: return std::bit_cast<uint32_t>(m_mask);
            default:
                throw Util::Error("No MI register found for addr " HEXFMT32, addr);
        }
    };

    auto data = readReg(addr);

    logOperation<Sys::MI, MI_REG_ADDR>(m_logger, "read", addr, data);

    return data;
}

auto MipsInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    MI_REG_ADDR::BASE <= addr && addr <= MI_REG_ADDR::END);

    addr = MI_REG_ADDR::BASE + (addr & 0xF);
    logOperation<Sys::MI, MI_REG_ADDR>(m_logger, "write", addr, data);

    switch (addr) {
        case MI_REG_ADDR::MI_MODE: {
            auto       newMode = m_mode.load();
            const auto mode    = std::bit_cast<MI_MODE::Write>(data);
            if (mode.setRepeat) {
                newMode.repeat      = 1;
                newMode.repeatCount = mode.repeatCount;
            }
            if (mode.clearRepeat) newMode.repeat = 0;
            if (mode.setEBus) newMode.eBus = 1;
            if (mode.clearDp) setInterrupt<^^MI_INTERRUPT::dp>(false);
            if (mode.setUpper) newMode.upper = 1;
            if (mode.clearUpper) newMode.upper = 0;
            m_mode.store(newMode);
            return;
        }
        case MI_REG_ADDR::MI_VERSION: [[fallthrough]];
        case MI_REG_ADDR::MI_INTERRUPT: {
            logWarnOnWriteToReadOnlyRegister<Sys::MI, MI_REG_ADDR>(m_logger, addr);
            return;
        }
        case MI_REG_ADDR::MI_MASK: {
            auto       newMask = m_mask.load();
            const auto mask    = std::bit_cast<MI_MASK::Write>(data);
            if (mask.clearSp) newMask.sp = 0;
            if (mask.setSp) newMask.sp = 1;
            if (mask.clearSi) newMask.si = 0;
            if (mask.setSi) newMask.si = 1;
            if (mask.clearAi) newMask.ai = 0;
            if (mask.setAi) newMask.ai = 1;
            if (mask.clearVi) newMask.vi = 0;
            if (mask.setVi) newMask.vi = 1;
            if (mask.clearPi) newMask.pi = 0;
            if (mask.setPi) newMask.pi = 1;
            if (mask.clearDp) newMask.dp = 0;
            if (mask.setDp) newMask.dp = 1;
            m_mask.store(newMask);
            updateInterrupt();
            return;
        }
    }
}

} // namespace Interfaces