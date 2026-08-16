export module Interfaces:RspRegisters;

import std;
import Util;

import :MmioRegisters;
import :RspRegistersTypes;

namespace Interfaces {

export class RspRegisters : public MmioRegisters {

    using RegisterPtr = std::variant<RSP_DMA_SPADDR*, RSP_DMA_RAMADDR*, RSP_DMA_RDLEN*, RSP_DMA_WRLEN*, RSP_STATUS*, RSP_DMA_FULL*, RSP_DMA_BUSY*, RSP_SEMAPHORE*, RSP_PC*>;

  public:
    RspRegisters(std::shared_ptr<Util::Logger> logger)
        : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;

    auto write(uint32_t addr, uint32_t data) -> void override;

    auto getReg(uint32_t addr) -> RegisterPtr //
        pre(RSP_REG_ADDR::BASE <= addr && addr <= RSP_REG_ADDR::END);

  private:
    rspRegs m_regs{};

    std::shared_ptr<Util::Logger> m_logger;
};

auto RspRegisters::read(uint32_t addr) -> uint32_t {
    return getReg(addr).visit([=, this](auto&& v) {
        auto value = std::bit_cast<uint32_t>(*v);
        if (m_logger && m_logger->enabled()) {
            m_logger->log<Util::Verbosity::MED>(
                std::tuple{"sys", "RSP_REG"},
                std::tuple{"op", "read"},
                std::tuple{"reg", "{}", std::meta::display_string_of(^^decltype(v))},
                std::tuple{"data", "0x{:08x}", value});
        }
        return value;
    });
}

auto RspRegisters::write(uint32_t addr, uint32_t data) -> void {
    getReg(addr).visit([=, this](auto&& v) {
        if (m_logger && m_logger->enabled()) {
            m_logger->log<Util::Verbosity::MED>(
                std::tuple{"sys", "RSP_REG"},
                std::tuple{"op", "write"},
                std::tuple{"reg", "{}", std::meta::display_string_of(^^decltype(v))},
                std::tuple{"data", "0x{:08x}", data});
            m_logger->log<Util::Verbosity::MED>(
                std::tuple{"sys", "RSP_REG"},
                std::tuple{"warning", "Ignoring RSP register write"});
        }
        return 0;
    });
}

auto RspRegisters::getReg(uint32_t addr) -> RspRegisters::RegisterPtr {
    switch (addr) {
        case RSP_REG_ADDR::RSP_DMA_SPADDR:
            return &(m_regs.dmaSpAddr);
        case RSP_REG_ADDR::RSP_DMA_RAMADDR:
            return &(m_regs.dmaRamAddr);
        case RSP_REG_ADDR::RSP_DMA_RDLEN:
            return &(m_regs.dmaRdLen);
        case RSP_REG_ADDR::RSP_DMA_WRLEN:
            return &(m_regs.dmaWrLen);
        case RSP_REG_ADDR::RSP_STATUS:
            return &(m_regs.status);
        case RSP_REG_ADDR::RSP_DMA_FULL:
            return &(m_regs.dmaFull);
        case RSP_REG_ADDR::RSP_DMA_BUSY:
            return &(m_regs.dmaBusy);
        case RSP_REG_ADDR::RSP_SEMAPHORE:
            return &(m_regs.semaphore);
        case RSP_REG_ADDR::RSP_PC:
            return &(m_regs.pc);
        default:
            throw std::runtime_error(std::format("No RSP register found for addr {:#08x}", addr));
    };
}

} // namespace Interfaces