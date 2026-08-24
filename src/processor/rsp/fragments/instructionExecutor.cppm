module;

#include <util/defines.hpp>

export module RSP:InstructionExecutor;

import std;
import CPU;
import ISA;
import Memory;
import Util;

import :Registers;

export namespace RSP {

class InstructionExecutor {
  public:
    InstructionExecutor(std::shared_ptr<Util::Logger> logger, CPU::Registers<Sys::RSP>* gprs, RSP::Registers* vprs, Memory::Memory* memory)
        : m_logger(logger), m_gprs(gprs), m_vprs(vprs), m_memory(memory), m_cpuExec(logger, gprs, memory) {}

    auto cpuExec() {
        return &m_cpuExec;
    }

    template <typename Function>
        requires std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>
    auto executeBivariate(uint32_t inst, Function&& func) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    CPU::Registers<Sys::RSP>*     m_gprs{};
    RSP::Registers*               m_vprs{};
    Memory::Memory*               m_memory{};

    CPU::InstructionExecutor<Sys::RSP> m_cpuExec;
};

template <typename Function>
    requires std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>
auto InstructionExecutor::executeBivariate(uint32_t inst, Function&& func) -> void {
    const auto op    = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vsVpr = m_vprs->readVpr(op.vs);
    const auto vtVpr = m_vprs->readVpr(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));

    auto result = RSP::Registers::VPR{};
    for (auto i = 0uz; i < 8; ++i) {
        result[i] = func(vsVpr[i], vtVpr[i]);
    }
    m_vprs->writeVpr(op.vd, result);
}
} // namespace RSP