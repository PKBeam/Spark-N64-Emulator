module;
#include <util/defines.hpp>
export module RspControl:RspControl;

import std;
import MemoryTypes;
import InterfaceTypes;
import RdpControl;
import Util;

using namespace std::string_view_literals;
using namespace Interfaces;

enum class RSP_DMA_DIRECTION : bool {
    TO_RDRAM   = 0,
    FROM_RDRAM = 1,
};
STD_FORMATTER_ENUM_NAME(RSP_DMA_DIRECTION);

enum class RSP_CP0_REGS : uint8_t {
    SP_DMA_SPADDR  = 0,
    SP_DMA_RAMADDR = 1,
    SP_DMA_RDLEN   = 2,
    SP_DMA_WRLEN   = 3,
    SP_STATUS      = 4,
    SP_DMA_FULL    = 5,
    SP_DMA_BUSY    = 6,
    SP_SEMAPHORE   = 7,
    DPC_START      = 8,
    DPC_END        = 9,
    DPC_CURRENT    = 10,
    DPC_STATUS     = 11,
    DPC_CLOCK      = 12,
    DPC_BUF_BUSY   = 13,
    DPC_PIPE_BUSY  = 14,
    DPC_TMEM_BUSY  = 15,
};
STD_FORMATTER_ENUM_NAME(RSP_CP0_REGS);

constexpr auto RSP_MEM_BASE = Util::rangeOf(Memory::PhysSeg::RSP_DMEM).lower;

export namespace RSP {

class Control {
  public:
    using SignalSet = std::bitset<8>;

    Control(std::shared_ptr<Util::Logger> logger, Memory::Memory* memory, RDP::Control* rdpControl) : m_logger(logger), m_memory(memory), m_rdpControl(rdpControl) {};

    auto getSignal(std::size_t signal) const -> bool //
        pre(signal < SignalSet{}.size());

    auto setSignal(std::size_t signal, bool value) -> void //
        pre(signal < SignalSet{}.size());

    template <std::size_t signal>
        requires(signal < SignalSet{}.size())
    auto getSignal() const -> bool {
        return getSignal(signal);
    }

    template <std::size_t signal>
        requires(signal < SignalSet{}.size())
    auto setSignal(bool value) -> void {
        setSignal(signal, value);
    }

    auto readPc() const -> std::optional<uint32_t>;
    auto writePc(uint32_t pc) -> void //
        pre(m_halt);
    auto clearPc() -> void;

    auto getSingleStep() const -> bool;
    auto setSingleStep(bool value) -> void;

    auto getBroke() const -> bool;
    auto clearBroke() -> void;

    auto getHalt() const -> bool;
    auto setHalt(bool value) -> void;

    auto getIntBreak() const -> bool;
    auto setIntBreak(bool value) -> void;

    auto readRegister(std::size_t index) -> uint32_t;
    auto writeRegister(std::size_t index, uint32_t data) -> void;

  private:
    template <RSP_DMA_DIRECTION Dir>
    auto dmaMemcpy(uint32_t dst, uint32_t src, std::size_t len, uint8_t count, uint16_t skip) -> void;

    auto patchRspBootAntiPiracyCheck() -> void;

    std::shared_ptr<Util::Logger> m_logger;
    std::optional<uint32_t>       m_pc = 0;
    Memory::Memory*               m_memory{};
    RDP::Control*                 m_rdpControl{};
    SignalSet                     m_signals          = {};
    bool                          m_broke            = false;
    bool                          m_halt             = true;
    bool                          m_singleStep       = false;
    bool                          m_interruptOnBreak = false;
    bool                          m_semaphore        = 0;
    uint32_t                      m_rspAddr{};
    uint32_t                      m_ramAddr{};
    bool                          m_cic6105rspBootPatched = false; // TODO reset when new rom loaded
};

template <RSP_DMA_DIRECTION Dir>
auto Control::dmaMemcpy(uint32_t dst, uint32_t src, std::size_t len, uint8_t count, uint16_t skip) -> void {
    len += 1;
    if (len % 8 > 0) {
        len += 8 - (len % 8); // round up to next multiple of 8
    }
    for (auto row = 0; row < count + 1; ++row) {
        for (auto i = 0uz; i < len; i += 8) {
            m_memory->memcpy<uint64_t>(dst + i, src + i);
        }
        if constexpr (Dir == RSP_DMA_DIRECTION::TO_RDRAM) {
            dst += skip;
        } else {
            src += skip;
        }
    }
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sev::INFO, Sys::RSP_REG>(
            "DMA {} {} bytes from " HEXFMT32 " to " HEXFMT32 " ({} rows, skip {})", Dir, len, src, dst, count + 1, skip);
        if (src <= dst && src + len >= dst) {
            m_logger->log<Level::MAX, Sev::WARNING, Sys::RSP_REG>("DMA address overlapped");
        }
    }
}

auto Control::readRegister(std::size_t index) -> uint32_t {
    auto readReg = [this](uint32_t index) -> uint32_t {
        switch (static_cast<RSP_CP0_REGS>(index)) {
            case RSP_CP0_REGS::SP_DMA_SPADDR:
                return m_rspAddr;
            case RSP_CP0_REGS::SP_DMA_RAMADDR:
                return m_ramAddr;
            case RSP_CP0_REGS::SP_DMA_RDLEN:
                return std::bit_cast<uint32_t>(SP_DMA_RDLEN{
                    .rdlen     = 0xFF8,
                    .count     = 0,
                    .skip_11_3 = 0, // TODO
                });
            case RSP_CP0_REGS::SP_DMA_WRLEN:
                return std::bit_cast<uint32_t>(SP_DMA_WRLEN{
                    .wrlen     = 0xFF8,
                    .count     = 0,
                    .skip_11_3 = 0, // TODO
                });
            case RSP_CP0_REGS::SP_STATUS:
                return std::bit_cast<uint32_t>(SP_STATUS{
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

            case RSP_CP0_REGS::SP_DMA_FULL:
                return 0;
            case RSP_CP0_REGS::SP_DMA_BUSY:
                return 0;
            case RSP_CP0_REGS::SP_SEMAPHORE: {
                const auto old = m_semaphore;
                m_semaphore    = 1;
                return old;
            }
            case RSP_CP0_REGS::DPC_START: [[fallthrough]];
            case RSP_CP0_REGS::DPC_END: [[fallthrough]];
            case RSP_CP0_REGS::DPC_CURRENT: [[fallthrough]];
            case RSP_CP0_REGS::DPC_STATUS: [[fallthrough]];
            case RSP_CP0_REGS::DPC_CLOCK: [[fallthrough]];
            case RSP_CP0_REGS::DPC_BUF_BUSY: [[fallthrough]];
            case RSP_CP0_REGS::DPC_PIPE_BUSY: [[fallthrough]];
            case RSP_CP0_REGS::DPC_TMEM_BUSY:
                return m_rdpControl->readRegister(static_cast<RDP::CMD_REGS>(index - static_cast<uint8_t>(RSP_CP0_REGS::DPC_START)));
            default:
                throw Util::Error("Invalid RSP control register index {}", index);
        }
    };

    const auto data = readReg(index);

    IF_LOG_ENABLED(m_logger) {
        const auto name = Util::enumName(static_cast<RSP_CP0_REGS>(index)).value_or(std::format("CP0 REG {}", index));
        m_logger->log<Level::HIGH, Sys::RSP_REG>(
            std::tuple{"op", "r"},
            std::tuple{std::format("{}", name), HEXFMT32, data});
    }
    return data;
}

auto Control::writeRegister(std::size_t index, uint32_t data) -> void {
    IF_LOG_ENABLED(m_logger) {
        const auto name = Util::enumName(static_cast<RSP_CP0_REGS>(index)).value_or(std::format("CP0 REG {}", index));
        m_logger->log<Level::HIGH, Sys::RSP_REG>(
            std::tuple{"op", "w"},
            std::tuple{std::format("{}", name), HEXFMT32, data});
    }

    switch (static_cast<RSP_CP0_REGS>(index)) {
        case RSP_CP0_REGS::SP_DMA_SPADDR: {
            auto spAddr = std::bit_cast<SP_DMA_SPADDR>(data);
            m_rspAddr   = (spAddr.memBank << 12) | (spAddr.memAddr_11_3 << 3);
            return;
        }
        case RSP_CP0_REGS::SP_DMA_RAMADDR: m_ramAddr = std::bit_cast<SP_DMA_RAMADDR>(data).dramAddr_23_3 << 3; return;
        case RSP_CP0_REGS::SP_DMA_RDLEN: {
            auto rdlen = std::bit_cast<SP_DMA_RDLEN>(data);
            dmaMemcpy<RSP_DMA_DIRECTION::FROM_RDRAM>(m_rspAddr + RSP_MEM_BASE, m_ramAddr, rdlen.rdlen, rdlen.count, rdlen.skip_11_3 << 3);
            // if (m_ramAddr == 0x001a1a40) {
            //     throw Util::Error("Test");
            // }
            patchRspBootAntiPiracyCheck();
            break;
        }
        case RSP_CP0_REGS::SP_DMA_WRLEN: {
            auto wrlen = std::bit_cast<SP_DMA_WRLEN>(data);
            dmaMemcpy<RSP_DMA_DIRECTION::TO_RDRAM>(m_ramAddr, m_rspAddr + RSP_MEM_BASE, wrlen.wrlen, wrlen.count, wrlen.skip_11_3 << 3);
            break;
        }
        case RSP_CP0_REGS::SP_STATUS: {
            auto status = std::bit_cast<SP_STATUS::Write>(data);
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
            break;
        }
        case RSP_CP0_REGS::SP_DMA_FULL: [[fallthrough]];
        case RSP_CP0_REGS::SP_DMA_BUSY:
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RSP_REG>("Ignoring write to read-only register {}", index);
            }
            break;
        case RSP_CP0_REGS::SP_SEMAPHORE:
            m_semaphore = std::bit_cast<SP_SEMAPHORE>(data).semaphore;
            break;
        case RSP_CP0_REGS::DPC_START: [[fallthrough]];
        case RSP_CP0_REGS::DPC_END: [[fallthrough]];
        case RSP_CP0_REGS::DPC_CURRENT: [[fallthrough]];
        case RSP_CP0_REGS::DPC_STATUS: [[fallthrough]];
        case RSP_CP0_REGS::DPC_CLOCK: [[fallthrough]];
        case RSP_CP0_REGS::DPC_BUF_BUSY: [[fallthrough]];
        case RSP_CP0_REGS::DPC_PIPE_BUSY: [[fallthrough]];
        case RSP_CP0_REGS::DPC_TMEM_BUSY:
            m_rdpControl->writeRegister(static_cast<RDP::CMD_REGS>(index - static_cast<uint8_t>(RSP_CP0_REGS::DPC_START)), data);
            break;
        default:
            throw Util::Error("No RSP register found for index {}", index);
    }
}

auto Control::readPc() const -> std::optional<uint32_t> {
    return m_pc;
}

auto Control::writePc(uint32_t pc) -> void {
    m_pc = pc;
}

auto Control::clearPc() -> void {
    m_pc.reset();
}

auto Control::getSignal(std::size_t signal) const -> bool {
    contract_assert(signal < m_signals.size());
    return m_signals[signal];
}

auto Control::setSignal(std::size_t signal, bool value) -> void {
    contract_assert(signal < m_signals.size());
    m_signals[signal] = value;
}

auto Control::getSingleStep() const -> bool {
    return m_singleStep;
}

auto Control::setSingleStep(bool value) -> void {
    m_singleStep = value;
}

auto Control::getBroke() const -> bool {
    return m_broke;
}

auto Control::clearBroke() -> void {
    m_broke = false;
}

auto Control::getHalt() const -> bool {
    return m_halt;
}

auto Control::setHalt(bool value) -> void {
    m_halt = value;
}

auto Control::getIntBreak() const -> bool {
    return m_interruptOnBreak;
}

auto Control::setIntBreak(bool value) -> void {
    m_interruptOnBreak = value;
}

auto Control::patchRspBootAntiPiracyCheck() -> void {
    // Patch CIC-6105 RSP boot anti piracy check
    if (m_rspAddr == 0x1000) {
        if (m_memory->read<uint32_t>(m_ramAddr) == 0x08000411 /* J  0x411 */) {
            // patch out branch checks
            constexpr auto BranchOffsets = std::array<uint32_t, 5>{0x04c, 0x05c, 0x068, 0x074, 0x07c};
            for (const auto offset : BranchOffsets) {
                m_memory->write<uint32_t>(RSP_MEM_BASE + m_rspAddr + offset, 0x00000000 /* NOP */);
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::MAX, Sev::WARNING, Sys::RSP_REG>("Patched out the CIC-6105 anti-piracy check in RSP boot code");
            }
            m_cic6105rspBootPatched = true;
        }
    }
}

} // namespace RSP
