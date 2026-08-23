module;
#include <util/defines.hpp>
export module RspControl:RspControl;

import std;
import InterfaceTypes;
import Util;

using namespace Interfaces;
enum class RspDmaDirection : bool {
    TO_RDRAM   = 0,
    FROM_RDRAM = 1,
};

export namespace RSP {

class Control {
  public:
    using SignalSet = std::bitset<8>;

    Control(std::shared_ptr<Util::Logger> logger, std::byte* memory) : m_logger(logger), m_memory(memory) {};

    auto setSignal(std::size_t signal, bool value) -> void //
        pre(signal < SignalSet{}.size());

    auto getSignal(std::size_t signal) -> bool //
        pre(signal < SignalSet{}.size());

    template <std::size_t signal>
        requires(signal < SignalSet{}.size())
    auto setSignal(bool value) -> void {
        setSignal(signal, value);
    }

    template <std::size_t signal>
        requires(signal < SignalSet{}.size())
    auto getSignal() -> bool {
        return getSignal(signal);
    }

    auto setPc(uint32_t pc) -> void //
        pre(m_halt);
    auto getPc() -> std::optional<uint32_t>;
    auto clearPc() -> void;
    auto incrementPc() -> void;
    auto setSingleStep(bool value) -> void;
    auto getSingleStep() -> bool;
    auto clearBroke() -> void;
    auto getBroke() -> bool;
    auto getHalt() -> bool;
    auto setHalt(bool value) -> void;
    auto setIntBreak(bool value) -> void;
    auto getIntBreak() -> bool;

    auto readRegister(std::size_t index) -> uint32_t;
    auto writeRegister(std::size_t index, uint32_t data) -> void;

  private:
    template <RspDmaDirection Dir>
    auto dmaMemcpy(uint32_t dst, uint32_t src, std::size_t len, uint8_t count, uint16_t skip) -> void;

    std::shared_ptr<Util::Logger> m_logger;
    std::optional<uint32_t>       m_pc = 0;
    std::byte*                    m_memory{};
    SignalSet                     m_signals          = {};
    bool                          m_broke            = false;
    bool                          m_halt             = true;
    bool                          m_singleStep       = false;
    bool                          m_interruptOnBreak = false;
    uint32_t                      m_rspAddr{};
    uint32_t                      m_ramAddr{};
};

template <RspDmaDirection Dir>
auto Control::dmaMemcpy(uint32_t dst, uint32_t src, std::size_t len, uint8_t count, uint16_t skip) -> void {
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

auto Control::readRegister(std::size_t index) -> uint32_t {
    switch (index) {
        case 0: // RSP_DMA_SPADDR
            return m_rspAddr;
        case 1: // RSP_DMA_RAMADDR
            return m_ramAddr;
        case 2: // RSP_DMA_RDLEN
            return std::bit_cast<uint32_t>(RSP_DMA_RDLEN{
                .rdlen     = 0xFF8,
                .count     = 0,
                .skip_11_3 = 0, // TODO
            });
        case 3: // RSP_DMA_WRLEN
            return std::bit_cast<uint32_t>(RSP_DMA_WRLEN{
                .wrlen     = 0xFF8,
                .count     = 0,
                .skip_11_3 = 0, // TODO
            });
        case 4: // RSP_STATUS
            return std::bit_cast<uint32_t>(RSP_STATUS{
                .halted   = getHalt(),
                .broke    = getBroke(),
                .dmaBusy  = false,
                .dmaFull  = false,
                .ioBusy   = false,
                .sstep    = getSingleStep(),
                .intbreak = getIntBreak(),
                .sig0     = getSignal(0),
                .sig1     = getSignal(1),
                .sig2     = getSignal(2),
                .sig3     = getSignal(3),
                .sig4     = getSignal(4),
                .sig5     = getSignal(5),
                .sig6     = getSignal(6),
                .sig7     = getSignal(7),
            });

        case 5: // RSP_DMA_FULL
            return 0;
        case 6: // RSP_DMA_BUSY
            return 0;
        case 7: // RSP_SEMAPHORE
            // TODO
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RSP_REG>("Ignoring read of RSP_SEMAPHORE");
            }
            return 0;
        default:
            throw Util::Error("Invalid RSP control register index {}", index);
    }
}

auto Control::writeRegister(std::size_t index, uint32_t data) -> void {

    switch (index) {
        case 0: m_rspAddr = data; return;
        case 1: m_ramAddr = data; return;
        case 2: {
            auto rdlen = std::bit_cast<RSP_DMA_RDLEN>(data);
            dmaMemcpy<RspDmaDirection::FROM_RDRAM>(m_rspAddr, m_ramAddr, rdlen.rdlen, rdlen.count, rdlen.skip_11_3 << 3);
            break;
        }
        case 3: {
            auto wrlen = std::bit_cast<RSP_DMA_WRLEN>(data);
            dmaMemcpy<RspDmaDirection::TO_RDRAM>(m_rspAddr, m_ramAddr, wrlen.wrlen, wrlen.count, wrlen.skip_11_3 << 3);
            break;
        }
        case 4: {
            auto status = std::bit_cast<RSP_STATUS::Write>(data);
            if (status.clrHalt) setHalt(false);
            if (status.setHalt) setHalt(true);
            if (status.clrBroke) clearBroke();
            // clrIntr and setIntr are handled in Interfaces::RspRegisters
            if (status.clrSstep) setSingleStep(false);
            if (status.setSstep) setSingleStep(true);
            if (status.clrIntbreak) setIntBreak(false);
            if (status.setIntbreak) setIntBreak(true);
            if (status.clrSig0) setSignal(0, false);
            if (status.setSig0) setSignal(0, true);
            if (status.clrSig1) setSignal(1, false);
            if (status.setSig1) setSignal(1, true);
            if (status.clrSig2) setSignal(2, false);
            if (status.setSig2) setSignal(2, true);
            if (status.clrSig3) setSignal(3, false);
            if (status.setSig3) setSignal(3, true);
            if (status.clrSig4) setSignal(4, false);
            if (status.setSig4) setSignal(4, true);
            if (status.clrSig5) setSignal(5, false);
            if (status.setSig5) setSignal(5, true);
            if (status.clrSig6) setSignal(6, false);
            if (status.setSig6) setSignal(6, true);
            if (status.clrSig7) setSignal(7, false);
            if (status.setSig7) setSignal(7, true);
            return;
        }
        case 5: [[fallthrough]];
        case 6: [[fallthrough]];
        case 7:
            // TODO
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RSP_REG>("Ignoring write to RSP_SEMAPHORE");
            }
            break;
        default:
            throw Util::Error("No RSP register found for index {}", index);
    }
}

auto Control::setPc(uint32_t pc) -> void {
    m_pc = pc;
}

auto Control::getPc() -> std::optional<uint32_t> {
    return m_pc;
}

auto Control::clearPc() -> void {
    m_pc.reset();
}

auto Control::setSignal(std::size_t signal, bool value) -> void {
    contract_assert(signal < m_signals.size());
    m_signals[signal] = value;
}

auto Control::getSignal(std::size_t signal) -> bool {
    contract_assert(signal < m_signals.size());
    return m_signals[signal];
}

auto Control::setSingleStep(bool value) -> void {
    m_singleStep = value;
}

auto Control::getSingleStep() -> bool {
    return m_singleStep;
}

auto Control::clearBroke() -> void {
    m_broke = false;
}

auto Control::getBroke() -> bool {
    return m_broke;
}

auto Control::getHalt() -> bool {
    return m_halt;
}

auto Control::setHalt(bool value) -> void {
    m_halt = value;
}

auto Control::setIntBreak(bool value) -> void {
    m_interruptOnBreak = value;
}

auto Control::getIntBreak() -> bool {
    return m_interruptOnBreak;
}

} // namespace RSP
