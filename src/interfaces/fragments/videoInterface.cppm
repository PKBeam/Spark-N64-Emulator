module;
#include <util/defines.hpp>
export module Interfaces:VideoInterface;

import std;
import Util;

import :Interface;
import :VideoInterfaceTypes;

namespace Interfaces {

export class VideoInterface : public Interface {
  public:
    VideoInterface(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;

    VI_CTRL m_ctrl{};
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
                logWarnOnIgnoredRegister<VI_REG_ADDR>(m_logger, addr);
                return 0;
            default:
                throw Util::Error("No VI register found for addr {:#08x}", addr);
        }
    };

    auto data = readReg(addr);

    logOperation<VI_REG_ADDR>(m_logger, "read", addr, data);

    return data;
}

auto VideoInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    VI_REG_ADDR::BASE <= addr && addr <= VI_REG_ADDR::END);

    addr = VI_REG_ADDR::BASE + (addr & 0x3F);
    logOperation<VI_REG_ADDR>(m_logger, "write", addr, data);

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
            logWarnOnIgnoredRegister<VI_REG_ADDR>(m_logger, addr);
            return; // Placeholder
        default:
            throw Util::Error("No VI register found for addr {:#08x}", addr);
    }
}

} // namespace Interfaces