module;
#include <util/defines.hpp>
export module Interfaces:VideoInterface;

import std;
import Util;

import :Interface;
import :MipsInterface;
import InterfaceTypes;

namespace Interfaces {

export class VideoInterface : public Interface {
  public:
    VideoInterface(std::shared_ptr<Util::Logger> logger, MipsInterface* mipsInterface) : m_logger(logger), m_mipsInterface(mipsInterface) {}

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

#if defined(DETERMINISTIC_VI_INTERRUPTS)
    auto tick(std::size_t cycles) -> void {
        constexpr std::size_t RSP_CYCLES_PER_VI_INTERRUPT = 62500000 / 60;
        if (m_ctrl.type == 0) {
            return;
        }
        m_cycles += cycles;
        if (m_cycles >= RSP_CYCLES_PER_VI_INTERRUPT) {
            m_cycles = 0;
            m_mipsInterface->setInterrupt<^^Interfaces::MI_INTERRUPT::vi>(true);
        }
    }
#else
    auto processNextFrame() -> void {
        if (m_ctrl.type != 0) {
            m_tick = true;
        }
    }
    auto getInterrupt() -> bool {
        const auto old = std::atomic_exchange(&m_tick, false);
        return old;
    }
#endif

  private:
    std::shared_ptr<Util::Logger> m_logger;
    MipsInterface*                m_mipsInterface;
    VI_CTRL                       m_ctrl{};

#if defined(DETERMINISTIC_VI_INTERRUPTS)
    std::size_t m_cycles{};
#else
    std::atomic<bool> m_tick{};
#endif
};

auto VideoInterface::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    VI_REG_ADDR::BASE <= addr && addr <= VI_REG_ADDR::END);

    addr = VI_REG_ADDR::BASE + (addr & 0x3F);

    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case VI_REG_ADDR::VI_CTRL:
                return std::bit_cast<uint32_t>(m_ctrl);
            case VI_REG_ADDR::VI_ORIGIN: [[fallthrough]];
            case VI_REG_ADDR::VI_WIDTH: [[fallthrough]];
            case VI_REG_ADDR::VI_V_INTR: [[fallthrough]];
            case VI_REG_ADDR::VI_V_CURRENT: [[fallthrough]];
            case VI_REG_ADDR::VI_BURST: [[fallthrough]];
            case VI_REG_ADDR::VI_V_TOTAL: [[fallthrough]];
            case VI_REG_ADDR::VI_H_TOTAL: [[fallthrough]];
            case VI_REG_ADDR::VI_H_TOTAL_LEAP: [[fallthrough]];
            case VI_REG_ADDR::VI_H_VIDEO: [[fallthrough]];
            case VI_REG_ADDR::VI_V_VIDEO: [[fallthrough]];
            case VI_REG_ADDR::VI_V_BURST: [[fallthrough]];
            case VI_REG_ADDR::VI_X_SCALE: [[fallthrough]];
            case VI_REG_ADDR::VI_Y_SCALE: [[fallthrough]];
            case VI_REG_ADDR::VI_TEST_ADDR: [[fallthrough]];
            case VI_REG_ADDR::VI_STAGED_DATA:
                logWarnOnIgnoredRegister<Sys::VI, VI_REG_ADDR>(m_logger, addr);
                return 0;
            default:
                throw Util::Error("No VI register found for addr " HEXFMT32, addr);
        }
    };

    auto data = readReg(addr);

    if (addr == VI_REG_ADDR::VI_V_CURRENT) { // TODO clean up
        data = 2;
    }

    logOperation<Sys::VI, VI_REG_ADDR>(m_logger, "read", addr, data);

    return data;
}

auto VideoInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    VI_REG_ADDR::BASE <= addr && addr <= VI_REG_ADDR::END);

    addr = VI_REG_ADDR::BASE + (addr & 0x3F);
    logOperation<Sys::VI, VI_REG_ADDR>(m_logger, "write", addr, data);

    switch (addr) {
        case VI_REG_ADDR::VI_V_CURRENT:
            m_mipsInterface->setInterrupt<^^Interfaces::MI_INTERRUPT::vi>(false);
            break;
        case VI_REG_ADDR::VI_CTRL:
            m_ctrl = std::bit_cast<VI_CTRL>(data);
            break;
        case VI_REG_ADDR::VI_ORIGIN: [[fallthrough]];
        case VI_REG_ADDR::VI_WIDTH: [[fallthrough]];
        case VI_REG_ADDR::VI_V_INTR: [[fallthrough]];
        case VI_REG_ADDR::VI_BURST: [[fallthrough]];
        case VI_REG_ADDR::VI_V_TOTAL: [[fallthrough]];
        case VI_REG_ADDR::VI_H_TOTAL: [[fallthrough]];
        case VI_REG_ADDR::VI_H_TOTAL_LEAP: [[fallthrough]];
        case VI_REG_ADDR::VI_H_VIDEO: [[fallthrough]];
        case VI_REG_ADDR::VI_V_VIDEO: [[fallthrough]];
        case VI_REG_ADDR::VI_V_BURST: [[fallthrough]];
        case VI_REG_ADDR::VI_X_SCALE: [[fallthrough]];
        case VI_REG_ADDR::VI_Y_SCALE: [[fallthrough]];
        case VI_REG_ADDR::VI_TEST_ADDR: [[fallthrough]];
        case VI_REG_ADDR::VI_STAGED_DATA:
            logWarnOnIgnoredRegister<Sys::VI, VI_REG_ADDR>(m_logger, addr);
            return; // Placeholder
        default:
            throw Util::Error("No VI register found for addr " HEXFMT32, addr);
    }
}

} // namespace Interfaces