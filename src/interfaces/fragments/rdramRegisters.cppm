export module Interfaces:RdramRegisters;

import std;
import Util;

import :MmioRegisters;
import :RdramRegistersTypes;

namespace Interfaces {

export class RdramRegisters : public MmioRegisters {
  public:
    RdramRegisters(std::shared_ptr<Util::Logger> logger)
        : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;
};

auto RdramRegisters::read(uint32_t addr) -> uint32_t {
    contract_assert(RDRAM_REG_ADDR::BASE <= addr && addr <= RDRAM_REG_ADDR::END);
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(std::tuple{"warning", "Ignoring RDRAM register read"});
    }
    return 0;
}

auto RdramRegisters::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(RDRAM_REG_ADDR::BASE <= addr && addr <= RDRAM_REG_ADDR::END);
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Ignoring RDRAM register write"});
    }
}

} // namespace Interfaces