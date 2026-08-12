export module Interfaces:RdramRegisters;

import std;
import Util;

import :MmioRegisters;
import :RdramRegistersTypes;

namespace Interfaces {

export class RdramRegisters : public MmioRegisters {
    using RegisterPtr = std::variant<RDRAM_REG_DEVICE_TYPE*, RDRAM_REG_DEVICE_ID*, RDRAM_REG_DELAY*, RDRAM_REG_MODE*, RDRAM_REG_REF_INTERVAL*, RDRAM_REG_REF_ROW*, RDRAM_REG_RAS_INTERVAL*, RDRAM_REG_MIN_INTERVAL*, RDRAM_REG_ADDRESS_SELECT*, RDRAM_REG_DEVICE_MANUFACTURER*>;

  public:
    RdramRegisters(std::shared_ptr<Util::Logger> logger)
        : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;

    auto write(uint32_t addr, uint32_t data) -> void override;

    auto getReg(uint32_t addr) -> RegisterPtr //
        pre(RDRAM_REG_ADDR::BASE <= addr && addr <= RDRAM_REG_ADDR::END);

  private:
    rdramRegs m_regs{};

    std::shared_ptr<Util::Logger> m_logger;
};

auto RdramRegisters::read(uint32_t addr) -> uint32_t {
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(std::tuple{"warning", "Ignoring RDRAM register read"});
    }
    return 0;
}

auto RdramRegisters::write(uint32_t addr, uint32_t data) -> void {
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Ignoring RDRAM register write"});
    }
}

auto RdramRegisters::getReg(uint32_t addr) -> RdramRegisters::RegisterPtr {
    return {};
}

} // namespace Interfaces