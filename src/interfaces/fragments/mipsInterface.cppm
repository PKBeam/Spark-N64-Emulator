export module Interfaces:MipsInterface;

import std;
import Util;

import :Interface;
import :MipsInterfaceTypes;

namespace Interfaces {

export class MipsInterface : public Interface {
  public:
    MipsInterface(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;

    MI_MODE      m_mode{};
    MI_INTERRUPT m_interrupt{};
    MI_MASK      m_mask{};
};

auto MipsInterface::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    MI_REG_ADDR::BASE <= addr && addr <= MI_REG_ADDR::END);

    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case MI_REG_ADDR::MI_MODE: return std::bit_cast<uint32_t>(m_mode);
            case MI_REG_ADDR::MI_VERSION: return std::bit_cast<uint32_t>(MI_VERSION{});
            case MI_REG_ADDR::MI_INTERRUPT: return std::bit_cast<uint32_t>(m_interrupt);
            case MI_REG_ADDR::MI_MASK: return std::bit_cast<uint32_t>(m_mask);
            default:
                throw Util::Error("No MI register found for addr {:#08x}", addr);
        }
    };

    auto data = readReg(addr);

    logOperation<MI_REG_ADDR>(m_logger, "read", addr, data);

    return data;
}

auto MipsInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    MI_REG_ADDR::BASE <= addr && addr <= MI_REG_ADDR::END);

    logOperation<MI_REG_ADDR>(m_logger, "write", addr, data);

    switch (addr) {
        case MI_REG_ADDR::MI_MODE: {
            auto mode = std::bit_cast<MI_MODE::Write>(data);
            if (mode.setRepeat) {
                m_mode.repeat      = 1;
                m_mode.repeatCount = mode.repeatCount;
            }
            if (mode.clearRepeat) m_mode.repeat = 0;
            if (mode.setEBus) m_mode.eBus = 1;
            if (mode.clearDp) m_interrupt.dp = 0;
            if (mode.setUpper) m_mode.upper = 1;
            if (mode.clearUpper) m_mode.upper = 0;
            return;
        }
        case MI_REG_ADDR::MI_VERSION: [[fallthrough]];
        case MI_REG_ADDR::MI_INTERRUPT: {
            logWarnOnWriteToReadOnlyRegister<MI_REG_ADDR>(m_logger, addr);
            return;
        }
        case MI_REG_ADDR::MI_MASK: {
            auto mask = std::bit_cast<MI_MASK::Write>(data);
            if (mask.clearSp) m_mask.sp = 0;
            if (mask.setSp) m_mask.sp = 1;
            if (mask.clearSi) m_mask.si = 0;
            if (mask.setSi) m_mask.si = 1;
            if (mask.clearAi) m_mask.ai = 0;
            if (mask.setAi) m_mask.ai = 1;
            if (mask.clearVi) m_mask.vi = 0;
            if (mask.setVi) m_mask.vi = 1;
            if (mask.clearPi) m_mask.pi = 0;
            if (mask.setPi) m_mask.pi = 1;
            if (mask.clearDp) m_mask.dp = 0;
            if (mask.setDp) m_mask.dp = 1;
            return;
        }
    }
}

} // namespace Interfaces