module;
#include <util/defines.hpp>
export module Interfaces:RdpRegisters;

import std;
import Util;

import :Interface;
import RspControl;
import InterfaceTypes;

namespace Interfaces {

export class RdpRegisters : public Interface {
  public:
    RdpRegisters(std::shared_ptr<Util::Logger> logger,
                 RSP::Control*                 rsp)
        : m_logger(logger), m_rspCtrl(rsp) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    RSP::Control*                 m_rspCtrl{};
};

auto RdpRegisters::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    RDP_REG_ADDR::BASE <= addr && addr <= RDP_REG_ADDR::END);

    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case RDP_REG_ADDR::DPC_START: return m_rspCtrl->readRegister(8);
            case RDP_REG_ADDR::DPC_END: return m_rspCtrl->readRegister(9);
            case RDP_REG_ADDR::DPC_CURRENT: return m_rspCtrl->readRegister(10);
            case RDP_REG_ADDR::DPC_STATUS: return m_rspCtrl->readRegister(11);
            case RDP_REG_ADDR::DPC_CLOCK: return m_rspCtrl->readRegister(12);
            case RDP_REG_ADDR::DPC_CMD_BUSY: return m_rspCtrl->readRegister(13);
            case RDP_REG_ADDR::DPC_PIPE_BUSY: return m_rspCtrl->readRegister(14);
            case RDP_REG_ADDR::DPC_TMEM_BUSY: return m_rspCtrl->readRegister(15);
            case RDP_REG_ADDR::DPS_TBIST: [[fallthrough]];
            case RDP_REG_ADDR::DPS_TEST_MODE: [[fallthrough]];
            case RDP_REG_ADDR::DPS_BUFTEST_ADDR: [[fallthrough]];
            case RDP_REG_ADDR::DPS_BUFTEST_DATA:
                logWarnOnIgnoredRegister<Sys::RDP_REG, RDP_REG_ADDR>(m_logger, addr);
                return 0;
            default: throw Util::Error("No RDP register found for addr {:#08x}", addr);
        }
    };

    auto data = readReg(addr);

    logOperation<Sys::RDP_REG, RDP_REG_ADDR>(m_logger, "read", addr, data);

    return data;
}

auto RdpRegisters::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    RDP_REG_ADDR::BASE <= addr && addr <= RDP_REG_ADDR::END);

    logOperation<Sys::RDP_REG, RDP_REG_ADDR>(m_logger, "write", addr, data);

    switch (addr) {
        case RDP_REG_ADDR::DPC_START: m_rspCtrl->writeRegister(8, data); return;
        case RDP_REG_ADDR::DPC_END: m_rspCtrl->writeRegister(9, data); return;
        case RDP_REG_ADDR::DPC_CURRENT: m_rspCtrl->writeRegister(10, data); return;
        case RDP_REG_ADDR::DPC_STATUS: m_rspCtrl->writeRegister(11, data); return;
        case RDP_REG_ADDR::DPC_CLOCK: m_rspCtrl->writeRegister(12, data); return;
        case RDP_REG_ADDR::DPC_CMD_BUSY: m_rspCtrl->writeRegister(13, data); return;
        case RDP_REG_ADDR::DPC_PIPE_BUSY: m_rspCtrl->writeRegister(14, data); return;
        case RDP_REG_ADDR::DPC_TMEM_BUSY: m_rspCtrl->writeRegister(15, data); return;
        case RDP_REG_ADDR::DPS_TBIST: [[fallthrough]];
        case RDP_REG_ADDR::DPS_TEST_MODE: [[fallthrough]];
        case RDP_REG_ADDR::DPS_BUFTEST_ADDR: [[fallthrough]];
        case RDP_REG_ADDR::DPS_BUFTEST_DATA:
            logWarnOnIgnoredRegister<Sys::RDP_REG, RDP_REG_ADDR>(m_logger, addr);
            return;
        default:
            throw Util::Error("No RDP register found for addr {:#08x}", addr);
    }
}

} // namespace Interfaces