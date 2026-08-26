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
    VideoInterface(std::shared_ptr<Util::Logger> logger, MipsInterface* mipsInterface);

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

    auto hasTimerFired() const -> bool {
        return m_timerTick;
    }

    auto clearTimerFired() -> void {
        m_timerTick = false;
    }

  private:
    std::thread                   m_interruptGenerator;
    std::shared_ptr<Util::Logger> m_logger;
    bool                          m_timerTick{};
    MipsInterface*                m_mipsInterface;
    VI_CTRL                       m_ctrl{};
};

VideoInterface::VideoInterface(std::shared_ptr<Util::Logger> logger, MipsInterface* mipsInterface)
    : m_logger(logger), m_mipsInterface(mipsInterface) {
    m_interruptGenerator = std::thread([this]() {
        auto prev = std::chrono::high_resolution_clock::now();
        while (true) {
            const auto now     = std::chrono::high_resolution_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - prev).count();
            if (elapsed >= 16667) { // 60Hz
                prev        = now;
                m_timerTick = true;
            }
        }
    });
}

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
                throw Util::Error("No VI register found for addr {:#08x}", addr);
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

    if (addr == VI_REG_ADDR::VI_V_CURRENT) { // TODO clean up
        m_mipsInterface->setInterrupt<^^MI_INTERRUPT::vi>(false);
    }

    switch (addr) {
        case VI_REG_ADDR::VI_CTRL: [[fallthrough]];
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
            return; // Placeholder
        default:
            throw Util::Error("No VI register found for addr {:#08x}", addr);
    }
}

} // namespace Interfaces