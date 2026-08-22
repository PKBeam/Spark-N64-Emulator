module;
#include <util/defines.hpp>
export module Interfaces:RspRegisters;

import std;
import Util;

import :Interface;
import RspControl;
import :MipsInterface;
import :RspRegistersTypes;

namespace Interfaces {

enum class RspDmaDirection : bool {
    TO_RDRAM   = 0,
    FROM_RDRAM = 1,
};

export class RspRegisters : public Interface {
  public:
    RspRegisters(std::shared_ptr<Util::Logger> logger,
                 std::byte*                    memory,
                 MipsInterface*                mipsInterface,
                 RSP::Control*                 rsp)
        : m_logger(logger), m_mipsInterface(mipsInterface), m_rsp(rsp), m_memory(memory) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    template <RspDmaDirection Dir>
    auto dmaMemcpy(uint32_t dst, uint32_t src, std::size_t len, uint8_t count, uint16_t skip) -> void;

    std::shared_ptr<Util::Logger> m_logger;
    MipsInterface*                m_mipsInterface;
    RSP::Control*                 m_rsp;
    std::byte*                    m_memory;

    uint32_t m_rspAddr;
    uint32_t m_ramAddr;
};

template <RspDmaDirection Dir>
auto RspRegisters::dmaMemcpy(uint32_t dst, uint32_t src, std::size_t len, uint8_t count, uint16_t skip) -> void {
    auto dstPtr = m_memory + dst;
    auto srcPtr = m_memory + src;
    for (auto row = 0; row < count + 1; ++row) {
        for (auto i = 0uz; i < len + 1; i += 8) {
            std::memcpy(dstPtr + i, srcPtr + i, 8);
        }

        srcPtr += len + 1;
        dstPtr += len + 1;

        if constexpr (Dir == RspDmaDirection::TO_RDRAM) {
            dst += skip;
        } else {
            src += skip;
        }
    }
}

auto RspRegisters::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    RSP_REG_ADDR::BASE <= addr && addr <= RSP_REG_ADDR::END);

    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case RSP_REG_ADDR::RSP_DMA_SPADDR:
                return m_rspAddr;
            case RSP_REG_ADDR::RSP_DMA_RAMADDR:
                return m_ramAddr;
            case RSP_REG_ADDR::RSP_DMA_RDLEN:
                return std::bit_cast<uint32_t>(RSP_DMA_RDLEN{
                    .rdlen     = 0xFF8,
                    .count     = 0,
                    .skip_11_3 = 0, // TODO
                });
            case RSP_REG_ADDR::RSP_DMA_WRLEN:
                return std::bit_cast<uint32_t>(RSP_DMA_WRLEN{
                    .wrlen     = 0xFF8,
                    .count     = 0,
                    .skip_11_3 = 0, // TODO
                });
            case RSP_REG_ADDR::RSP_STATUS: {
                return std::bit_cast<uint32_t>(RSP_STATUS{
                    .halted   = m_rsp->getHalt(),
                    .broke    = m_rsp->getBroke(),
                    .dmaBusy  = false,
                    .dmaFull  = false,
                    .ioBusy   = false,
                    .sstep    = m_rsp->getSingleStep(),
                    .intbreak = m_rsp->getIntBreak(),
                    .sig0     = m_rsp->getSignal(0),
                    .sig1     = m_rsp->getSignal(1),
                    .sig2     = m_rsp->getSignal(2),
                    .sig3     = m_rsp->getSignal(3),
                    .sig4     = m_rsp->getSignal(4),
                    .sig5     = m_rsp->getSignal(5),
                    .sig6     = m_rsp->getSignal(6),
                    .sig7     = m_rsp->getSignal(7),
                });
            }
            case RSP_REG_ADDR::RSP_DMA_FULL: return 0;
            case RSP_REG_ADDR::RSP_DMA_BUSY: return 0;
            case RSP_REG_ADDR::RSP_SEMAPHORE:
                // TODO
                logWarnOnIgnoredRegister<RSP_REG_ADDR>(m_logger, addr);
                return 0;
            case RSP_REG_ADDR::RSP_PC:
                if (!m_rsp->getHalt()) {
                    IF_LOG_ENABLED(m_logger) {
                        m_logger->log<Util::Verbosity::MED>(
                            std::tuple{"sys", std::meta::display_string_of(^^decltype(*this))},
                            std::tuple{"warning", "Attempted to read PC while RSP is not halted"});
                    }
                }
                return m_rsp->getPc();
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
        case RSP_REG_ADDR::RSP_DMA_SPADDR: m_rspAddr = data; return;
        case RSP_REG_ADDR::RSP_DMA_RAMADDR: m_ramAddr = data; return;
        case RSP_REG_ADDR::RSP_DMA_RDLEN: {
            auto rdlen = std::bit_cast<RSP_DMA_RDLEN>(data);
            dmaMemcpy<RspDmaDirection::FROM_RDRAM>(m_rspAddr, m_ramAddr, rdlen.rdlen, rdlen.count, rdlen.skip_11_3 << 3);
            break;
        }
        case RSP_REG_ADDR::RSP_DMA_WRLEN: {
            auto wrlen = std::bit_cast<RSP_DMA_WRLEN>(data);
            dmaMemcpy<RspDmaDirection::TO_RDRAM>(m_rspAddr, m_ramAddr, wrlen.wrlen, wrlen.count, wrlen.skip_11_3 << 3);
            break;
        }
        case RSP_REG_ADDR::RSP_STATUS: {
            auto status = std::bit_cast<RSP_STATUS::Write>(data);
            if (status.clrHalt) m_rsp->setHalt(false);
            if (status.setHalt) m_rsp->setHalt(true);
            if (status.clrBroke) m_rsp->clearBroke();
            if (status.clrIntr) m_mipsInterface->setInterrupt<^^MI_INTERRUPT::sp>(false);
            if (status.setIntr) m_mipsInterface->setInterrupt<^^MI_INTERRUPT::sp>(true);
            if (status.clrSstep) m_rsp->setSingleStep(false);
            if (status.setSstep) m_rsp->setSingleStep(true);
            if (status.clrIntbreak) m_rsp->setIntBreak(false);
            if (status.setIntbreak) m_rsp->setIntBreak(true);
            if (status.clrSig0) m_rsp->setSignal(0, false);
            if (status.setSig0) m_rsp->setSignal(0, true);
            if (status.clrSig1) m_rsp->setSignal(1, false);
            if (status.setSig1) m_rsp->setSignal(1, true);
            if (status.clrSig2) m_rsp->setSignal(2, false);
            if (status.setSig2) m_rsp->setSignal(2, true);
            if (status.clrSig3) m_rsp->setSignal(3, false);
            if (status.setSig3) m_rsp->setSignal(3, true);
            if (status.clrSig4) m_rsp->setSignal(4, false);
            if (status.setSig4) m_rsp->setSignal(4, true);
            if (status.clrSig5) m_rsp->setSignal(5, false);
            if (status.setSig5) m_rsp->setSignal(5, true);
            if (status.clrSig6) m_rsp->setSignal(6, false);
            if (status.setSig6) m_rsp->setSignal(6, true);
            if (status.clrSig7) m_rsp->setSignal(7, false);
            if (status.setSig7) m_rsp->setSignal(7, true);
            return;
        }
        case RSP_REG_ADDR::RSP_DMA_FULL: [[fallthrough]];
        case RSP_REG_ADDR::RSP_DMA_BUSY: [[fallthrough]];
        case RSP_REG_ADDR::RSP_SEMAPHORE:
            // TODO
            logWarnOnIgnoredRegister<RSP_REG_ADDR>(m_logger, addr);
            break;
        case RSP_REG_ADDR::RSP_PC:
            m_rsp->setPc(data);
            return;
        default:
            throw Util::Error("No RSP register found for addr {:#08x}", addr);
    }
}

} // namespace Interfaces