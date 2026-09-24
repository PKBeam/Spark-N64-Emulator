module;
#include <util/defines.hpp>
export module RdpControl:RdpControl;

import std;
import InterfaceTypes;
import MemoryTypes;
import Util;

using namespace Interfaces;

constexpr auto RSP_DMEM_BASE = Util::rangeOf(Memory::PhysSeg::RSP_DMEM).lower;

export namespace RDP {
enum class CMD_REGS : uint8_t {
    DPC_START = 0,
    DPC_END,
    DPC_CURRENT,
    DPC_STATUS,
    DPC_CLOCK,
    DPC_CMD_BUSY,
    DPC_PIPE_BUSY,
    DPC_TMEM_BUSY,
    DPS_TBIST,
    DPS_TEST_MODE,
    DPS_BUFTEST_ADDR,
    DPS_BUFTEST_DATA,
};
};
STD_FORMATTER_ENUM_NAME(RDP::CMD_REGS);

export namespace RDP {
class Control {
  public:
    Control(std::shared_ptr<Util::Logger> logger, Memory::Memory* memory) : m_logger(logger), m_memory(memory) {};

    auto readRegister(CMD_REGS index) -> uint32_t;
    auto writeRegister(CMD_REGS index, uint32_t data) -> void;

    auto hasCommands() const -> bool;

    auto getCommands() -> std::deque<uint64_t>&; // may be called on a different thread but only by one caller

  private:
    auto dmaCommands() -> void; // fetch commands from RDRAM/DMEM

    std::shared_ptr<Util::Logger> m_logger;
    Memory::Memory*               m_memory{};

    std::deque<uint64_t> m_cmdBufferIn;  // From Memory
    std::deque<uint64_t> m_cmdBufferOut; // To RDP
    std::mutex           m_mutex;

    std::atomic<bool> m_startPending{};
    std::atomic<bool> m_endPending{};

    DPC_STATUS            m_status{};
    uint32_t              m_startAddr{};
    uint32_t              m_endAddr{};
    uint32_t              m_clock{};
    std::atomic<uint32_t> m_current{};
};

auto Control::hasCommands() const -> bool {
    return !m_cmdBufferIn.empty();
}

auto Control::getCommands() -> std::deque<uint64_t>& {
    if (!hasCommands()) {
        return m_cmdBufferOut;
    }

    { // lock commands
        auto lock = std::scoped_lock(m_mutex);
        std::swap(m_cmdBufferIn, m_cmdBufferOut);
        m_cmdBufferIn.clear();
    }
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::LOW, Sev::INFO, Sys::RDP>("Command buffer emptied by RDP");
    }
    return m_cmdBufferOut;
}

auto Control::dmaCommands() -> void {
    auto numCommands = 0;
    auto baseAddr    = 0uz;
    {
        auto lock = std::scoped_lock(m_mutex);

        numCommands = (static_cast<int32_t>(m_endAddr) - static_cast<int32_t>(m_startAddr)) / 8;
        if (numCommands < 0) {
            throw Util::Error("RDP command endAddr (" HEXFMT32 ") was before startAddr (" HEXFMT32 ")", m_endAddr, m_startAddr);
        } else if (numCommands == 0) {
            return;
        }
        baseAddr = m_startAddr + (m_status.xbus == 0 ? 0 : RSP_DMEM_BASE);
        for (auto offset : std::views::iota(0, numCommands)) {
            const auto cmd = m_memory->read<uint64_t>(baseAddr + offset * 8);
            m_cmdBufferIn.push_back(cmd);
        }
        m_current = m_endAddr;
    }

    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::LOW, Sev::INFO, Sys::RDP>("DMA {} commands from " HEXFMT32 " into command buffer", numCommands, baseAddr);
    }
}

auto Control::readRegister(CMD_REGS index) -> uint32_t {
    auto readReg = [this](CMD_REGS index) -> uint32_t {
        switch (index) {
            case CMD_REGS::DPC_START: return m_startAddr & 0x00FFFFFF;
            case CMD_REGS::DPC_END: return m_endAddr & 0x00FFFFFF;
            case CMD_REGS::DPC_CURRENT: return m_current & 0x00FFFFFF;
            case CMD_REGS::DPC_STATUS: {

                if (!hasCommands() && !m_endPending) { // check for recently finished DMAs
                    m_startPending = false;
                }
                if (m_endPending) { // begin the next pending transfer
                    dmaCommands();
                    m_startPending = false;
                    m_endPending   = false;
                }

                auto status         = m_status;
                status.startPending = m_startPending ? 1 : 0;
                status.endPending   = m_endPending ? 1 : 0;
                status.cmdBusy      = hasCommands() ? 1 : 0;
                status.tmemBusy     = hasCommands() ? 1 : 0;
                status.pipeBusy     = hasCommands() ? 1 : 0;
                return std::bit_cast<uint32_t>(status);
            }
            case CMD_REGS::DPC_CLOCK: return m_clock & 0x00FFFFFF;
            case CMD_REGS::DPC_CMD_BUSY: [[fallthrough]];
            case CMD_REGS::DPC_PIPE_BUSY: [[fallthrough]];
            case CMD_REGS::DPC_TMEM_BUSY: return hasCommands();
            case CMD_REGS::DPS_TBIST: [[fallthrough]];
            case CMD_REGS::DPS_TEST_MODE: [[fallthrough]];
            case CMD_REGS::DPS_BUFTEST_ADDR: [[fallthrough]];
            case CMD_REGS::DPS_BUFTEST_DATA:
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::HIGH, Sev::WARNING, Sys::RDP_REG>("Ignoring read from RDP register {}", static_cast<uint8_t>(index));
                }
                return 0;
            default:
                throw Util::Error("Invalid RDP control register index {}", index);
        }
    };

    const auto data = readReg(index);

    IF_LOG_ENABLED(m_logger) {
        const auto name = Util::enumName(static_cast<CMD_REGS>(index)).value_or(std::format("RDP REG {}", static_cast<uint8_t>(index)));
        m_logger->log<Level::HIGH, Sys::RDP_REG>(
            std::tuple{"op", "r"},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", HEXFMT32, data});
    }
    return data;
}

auto Control::writeRegister(CMD_REGS index, uint32_t data) -> void {
    IF_LOG_ENABLED(m_logger) {
        const auto name = Util::enumName(static_cast<CMD_REGS>(index)).value_or(std::format("RDP REG {}", static_cast<uint8_t>(index)));
        m_logger->log<Level::HIGH, Sys::RDP_REG>(
            std::tuple{"op", "w"},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", HEXFMT32, data});
    }

    switch (index) {
        case CMD_REGS::DPC_START:
            m_startAddr    = std::bit_cast<DPC_START>(data).start;
            m_startPending = true;
            return;
        case CMD_REGS::DPC_END: {
            if (m_startPending) { // start a new transfer
                m_endAddr = std::bit_cast<DPC_END>(data).end;
                if (hasCommands()) { // wait for transfer to finish
                    m_endPending = true;
                } else { // start transfer
                    dmaCommands();
                    m_startPending = false;
                }
            } else { // continue existing transfer
                m_startAddr = m_endAddr;
                m_endAddr   = std::bit_cast<DPC_END>(data).end;
                dmaCommands();
                m_endPending = false;
            }
            return;
        }
        case CMD_REGS::DPC_STATUS: {
            const auto status = std::bit_cast<DPC_STATUS::Write>(data);
            if (status.clrXbus) m_status.xbus = 0;
            if (status.setXbus) m_status.xbus = 1;
            if (status.clrFreeze) m_status.freeze = 0;
            if (status.setFreeze) m_status.freeze = 1;
            if (status.clrFlush) m_status.flush = 0;
            if (status.setFlush) m_status.flush = 1;
            // ignore status.clrTmemBusy
            // ignore status.clrPipeBusy
            // ignore status.clrBufferBusy
            if (status.clrClock) m_clock = 0;
            return;
        }
        case CMD_REGS::DPC_CURRENT: [[fallthrough]];
        case CMD_REGS::DPC_CLOCK: [[fallthrough]];
        case CMD_REGS::DPC_CMD_BUSY: [[fallthrough]];
        case CMD_REGS::DPC_PIPE_BUSY: [[fallthrough]];
        case CMD_REGS::DPC_TMEM_BUSY:
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RDP_REG>("Ignoring write to read-only register {}", static_cast<uint8_t>(index));
            };
            break;
        case CMD_REGS::DPS_TBIST: [[fallthrough]];
        case CMD_REGS::DPS_TEST_MODE: [[fallthrough]];
        case CMD_REGS::DPS_BUFTEST_ADDR: [[fallthrough]];
        case CMD_REGS::DPS_BUFTEST_DATA:
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RDP_REG>("Ignoring write to RDP register {}", static_cast<uint8_t>(index));
            }
            return;
        default:
            throw Util::Error("No RDP register found for index {}", index);
    }
}

} // namespace RDP
