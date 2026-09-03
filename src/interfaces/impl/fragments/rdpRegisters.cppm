module;
#include <util/defines.hpp>
export module Interfaces:RdpRegisters;

import std;
import Util;

import :Interface;
import RdpControl;
import InterfaceTypes;

namespace Interfaces {

export class RdpRegisters : public Interface {
  public:
    RdpRegisters(std::shared_ptr<Util::Logger> logger,
                 RDP::Control*                 rdp)
        : m_logger(logger), m_rdpCtrl(rdp) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    RDP::Control*                 m_rdpCtrl{};
};

auto RdpRegisters::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    RDP_REG_ADDR::BASE <= addr && addr <= RDP_REG_ADDR::END);
    switch (addr) {
        case RDP_REG_ADDR::DPC_START: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPC_START);
        case RDP_REG_ADDR::DPC_END: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPC_END);
        case RDP_REG_ADDR::DPC_CURRENT: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPC_CURRENT);
        case RDP_REG_ADDR::DPC_STATUS: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPC_STATUS);
        case RDP_REG_ADDR::DPC_CLOCK: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPC_CLOCK);
        case RDP_REG_ADDR::DPC_CMD_BUSY: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPC_CMD_BUSY);
        case RDP_REG_ADDR::DPC_PIPE_BUSY: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPC_PIPE_BUSY);
        case RDP_REG_ADDR::DPC_TMEM_BUSY: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPC_TMEM_BUSY);
        case RDP_REG_ADDR::DPS_TBIST: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPS_TBIST);
        case RDP_REG_ADDR::DPS_TEST_MODE: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPS_TEST_MODE);
        case RDP_REG_ADDR::DPS_BUFTEST_ADDR: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPS_BUFTEST_ADDR);
        case RDP_REG_ADDR::DPS_BUFTEST_DATA: return m_rdpCtrl->readRegister(RDP::CMD_REGS::DPS_BUFTEST_DATA);
        default: throw Util::Error("No RDP register found for addr " HEXFMT32, addr);
    }
}

auto RdpRegisters::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    RDP_REG_ADDR::BASE <= addr && addr <= RDP_REG_ADDR::END);
    switch (addr) {
        case RDP_REG_ADDR::DPC_START: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPC_START, data); return;
        case RDP_REG_ADDR::DPC_END: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPC_END, data); return;
        case RDP_REG_ADDR::DPC_CURRENT: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPC_CURRENT, data); return;
        case RDP_REG_ADDR::DPC_STATUS: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPC_STATUS, data); return;
        case RDP_REG_ADDR::DPC_CLOCK: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPC_CLOCK, data); return;
        case RDP_REG_ADDR::DPC_CMD_BUSY: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPC_CMD_BUSY, data); return;
        case RDP_REG_ADDR::DPC_PIPE_BUSY: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPC_PIPE_BUSY, data); return;
        case RDP_REG_ADDR::DPC_TMEM_BUSY: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPC_TMEM_BUSY, data); return;
        case RDP_REG_ADDR::DPS_TBIST: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPS_TBIST, data); return;
        case RDP_REG_ADDR::DPS_TEST_MODE: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPS_TEST_MODE, data); return;
        case RDP_REG_ADDR::DPS_BUFTEST_ADDR: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPS_BUFTEST_ADDR, data); return;
        case RDP_REG_ADDR::DPS_BUFTEST_DATA: m_rdpCtrl->writeRegister(RDP::CMD_REGS::DPS_BUFTEST_DATA, data); return;
        default:
            throw Util::Error("No RDP register found for addr " HEXFMT32, addr);
    }
}

} // namespace Interfaces