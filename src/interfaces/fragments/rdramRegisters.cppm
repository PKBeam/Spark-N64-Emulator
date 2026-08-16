export module Interfaces:RdramRegisters;

import std;
import Util;

import :Interface;
import :RdramRegistersTypes;

namespace Interfaces {

export class RdramRegisters : public Interface {
  public:
    RdramRegisters(std::shared_ptr<Util::Logger> logger)
        : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;
};

auto RdramRegisters::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    RDRAM_REG_ADDR::BASE <= addr && addr <= RDRAM_REG_ADDR::END);
    logWarnOnIgnoredRegister<RDRAM_REG_ADDR>(m_logger, addr);
    return 0;
}

auto RdramRegisters::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    RDRAM_REG_ADDR::BASE <= addr && addr <= RDRAM_REG_ADDR::END);
    logWarnOnIgnoredRegister<RDRAM_REG_ADDR>(m_logger, addr);
}

} // namespace Interfaces