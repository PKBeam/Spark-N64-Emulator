module;
#include <util/defines.hpp>
export module Interfaces:RspRegisters;

import std;
import Util;

import :Interface;
import RspControl;
import :MipsInterface;
import InterfaceTypes;

namespace Interfaces {

export class RspRegisters : public Interface {
  public:
    RspRegisters(std::shared_ptr<Util::Logger> logger,
                 MipsInterface*                mipsInterface,
                 RSP::Control*                 rsp)
        : m_logger(logger), m_mipsInterface(mipsInterface), m_rspCtrl(rsp) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    MipsInterface*                m_mipsInterface{};
    RSP::Control*                 m_rspCtrl{};
};

auto RspRegisters::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    RSP_REG_ADDR::BASE <= addr && addr <= RSP_REG_ADDR::END);
    switch (addr) {
        case RSP_REG_ADDR::SP_DMA_SPADDR: return m_rspCtrl->readRegister(0);
        case RSP_REG_ADDR::SP_DMA_RAMADDR: return m_rspCtrl->readRegister(1);
        case RSP_REG_ADDR::SP_DMA_RDLEN: return m_rspCtrl->readRegister(2);
        case RSP_REG_ADDR::SP_DMA_WRLEN: return m_rspCtrl->readRegister(3);
        case RSP_REG_ADDR::SP_STATUS: return m_rspCtrl->readRegister(4);
        case RSP_REG_ADDR::SP_DMA_FULL: return m_rspCtrl->readRegister(5);
        case RSP_REG_ADDR::SP_DMA_BUSY: return m_rspCtrl->readRegister(6);
        case RSP_REG_ADDR::SP_SEMAPHORE: return m_rspCtrl->readRegister(7);
        case RSP_REG_ADDR::SP_PC:
            if (!m_rspCtrl->getHalt()) {
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::HIGH, Sev::WARNING, Sys::RSP_REG>("Attempted to read PC while RSP is not halted");
                }
            }
            return m_rspCtrl->readPc().value_or(0);
        default:
            throw Util::Error("No RSP register found for addr " HEXFMT32, addr);
    }
}

auto RspRegisters::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    RSP_REG_ADDR::BASE <= addr && addr <= RSP_REG_ADDR::END);
    switch (addr) {
        case RSP_REG_ADDR::SP_DMA_SPADDR: m_rspCtrl->writeRegister(0, data); return;
        case RSP_REG_ADDR::SP_DMA_RAMADDR: m_rspCtrl->writeRegister(1, data); return;
        case RSP_REG_ADDR::SP_DMA_RDLEN: m_rspCtrl->writeRegister(2, data); return;
        case RSP_REG_ADDR::SP_DMA_WRLEN: m_rspCtrl->writeRegister(3, data); return;
        case RSP_REG_ADDR::SP_STATUS: {
            m_rspCtrl->writeRegister(4, data);
            // MIPS interface interrupts need to be handled inside this module
            auto status = std::bit_cast<SP_STATUS::Write>(data);
            if (status.clrIntr) m_mipsInterface->setInterrupt<^^MI_INTERRUPT::sp>(false);
            if (status.setIntr) m_mipsInterface->setInterrupt<^^MI_INTERRUPT::sp>(true);
            return;
        }
        case RSP_REG_ADDR::SP_DMA_FULL: m_rspCtrl->writeRegister(5, data); return;
        case RSP_REG_ADDR::SP_DMA_BUSY: m_rspCtrl->writeRegister(6, data); return;
        case RSP_REG_ADDR::SP_SEMAPHORE: m_rspCtrl->writeRegister(7, data); return;
        case RSP_REG_ADDR::SP_PC:
            m_rspCtrl->writePc(data & 0xFFF);
            return;
        default:
            throw Util::Error("No RSP register found for addr " HEXFMT32, addr);
    }
}

} // namespace Interfaces