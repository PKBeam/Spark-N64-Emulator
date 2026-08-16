export module Interfaces:RspRegisters;

import std;
import Util;

import :Interface;
import :RspRegistersTypes;

namespace Interfaces {

export class RspRegisters : public Interface {
  public:
    RspRegisters(std::shared_ptr<Util::Logger> logger)
        : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;

    RSP_STATUS m_status{};
};

auto RspRegisters::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    RSP_REG_ADDR::BASE <= addr && addr <= RSP_REG_ADDR::END);

    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case RSP_REG_ADDR::RSP_DMA_SPADDR: [[fallthrough]];
            case RSP_REG_ADDR::RSP_DMA_RAMADDR: [[fallthrough]];
            case RSP_REG_ADDR::RSP_DMA_RDLEN: [[fallthrough]];
            case RSP_REG_ADDR::RSP_DMA_WRLEN: [[fallthrough]];
            case RSP_REG_ADDR::RSP_STATUS:
                return std::bit_cast<uint32_t>(m_status);
            case RSP_REG_ADDR::RSP_DMA_FULL: [[fallthrough]];
            case RSP_REG_ADDR::RSP_DMA_BUSY: [[fallthrough]];
            case RSP_REG_ADDR::RSP_SEMAPHORE: [[fallthrough]];
            case RSP_REG_ADDR::RSP_PC:
                // TODO
                logWarnOnIgnoredRegister<RSP_REG_ADDR>(m_logger, addr);
                return 0;
            default:
                throw Util::Error("No RSP register found for addr {:#08x}", addr);
        }
    };

    auto data = readReg(addr);

    logOperation<RSP_REG_ADDR>(m_logger, "read", addr, data);

    return data;
}

auto RspRegisters::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    RSP_REG_ADDR::BASE <= addr && addr <= RSP_REG_ADDR::END);

    logOperation<RSP_REG_ADDR>(m_logger, "write", addr, data);

    switch (addr) {
        case RSP_REG_ADDR::RSP_DMA_SPADDR: [[fallthrough]];
        case RSP_REG_ADDR::RSP_DMA_RAMADDR: [[fallthrough]];
        case RSP_REG_ADDR::RSP_DMA_RDLEN: [[fallthrough]];
        case RSP_REG_ADDR::RSP_DMA_WRLEN: [[fallthrough]];
        case RSP_REG_ADDR::RSP_STATUS: {
            auto status = std::bit_cast<RSP_STATUS::Write>(data);
            if (status.clrHalt) m_status.halted = 0;
            if (status.setHalt) m_status.halted = 1;
            if (status.clrBroke) m_status.broke = 0;
            // if (status.clrIntr) TODO
            // if (status.setIntr) TODO
            if (status.clrSstep) m_status.sstep = 0;
            if (status.setSstep) m_status.sstep = 1;
            if (status.clrIntbreak) m_status.intbreak = 0;
            if (status.setIntbreak) m_status.intbreak = 1;
            if (status.clrSig0) m_status.sig0 = 0;
            if (status.setSig0) m_status.sig0 = 1;
            if (status.clrSig1) m_status.sig1 = 0;
            if (status.setSig1) m_status.sig1 = 1;
            if (status.clrSig2) m_status.sig2 = 0;
            if (status.setSig2) m_status.sig2 = 1;
            if (status.clrSig3) m_status.sig3 = 0;
            if (status.setSig3) m_status.sig3 = 1;
            if (status.clrSig4) m_status.sig4 = 0;
            if (status.setSig4) m_status.sig4 = 1;
            if (status.clrSig5) m_status.sig5 = 0;
            if (status.setSig5) m_status.sig5 = 1;
            if (status.clrSig6) m_status.sig6 = 0;
            if (status.setSig6) m_status.sig6 = 1;
            if (status.clrSig7) m_status.sig7 = 0;
            if (status.setSig7) m_status.sig7 = 1;
            return;
        }
        case RSP_REG_ADDR::RSP_DMA_FULL: [[fallthrough]];
        case RSP_REG_ADDR::RSP_DMA_BUSY: [[fallthrough]];
        case RSP_REG_ADDR::RSP_SEMAPHORE: [[fallthrough]];
        case RSP_REG_ADDR::RSP_PC:
            // TODO
            logWarnOnIgnoredRegister<RSP_REG_ADDR>(m_logger, addr);
            break;
        default:
            throw Util::Error("No RSP register found for addr {:#08x}", addr);
    }
}

} // namespace Interfaces