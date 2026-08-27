module;

#include <util/defines.hpp>

export module RSP:InstructionExecutor;

import std;
import CPU;
import ISA;
import Memory;
import Util;

import :Registers;

export namespace Param {
// clang-format off
enum Accumulator : uint8_t { ACCUM_NONE, ACCUM_ZERO_EXT, ACCUM_SIGN_EXT };
enum CarryIn     : uint8_t { CARRY_IN_NONE, CARRY_IN };
// clang-format on
} // namespace Param

export namespace RSP {

class InstructionExecutor {
  public:
    InstructionExecutor(std::shared_ptr<Util::Logger> logger, CPU::Registers<Sys::RSP>* gprs, RSP::Registers* vprs, Memory::Memory* memory)
        : m_logger(logger), m_gprs(gprs), m_vprs(vprs), m_memory(memory), m_cpuExec(logger, gprs, memory) {}

    auto cpuExec() {
        return &m_cpuExec;
    }

    template <Param::Accumulator Accum, typename VcoLoFunc = std::nullptr_t, typename VcoHiFunc = std::nullptr_t, Param::CarryIn Carry = Param::CARRY_IN_NONE, typename Function>
        requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>> &&
                 (std::same_as<std::nullptr_t, VcoLoFunc> || std::integral<std::invoke_result_t<VcoLoFunc, uint32_t>>) &&
                 (std::same_as<std::nullptr_t, VcoHiFunc> || std::integral<std::invoke_result_t<VcoHiFunc, uint32_t>>))
    auto executeBivariate(uint32_t inst, Function&& func, VcoLoFunc vcoLoFunc = nullptr, VcoHiFunc vcoHiFunc = nullptr) -> void;

    template <Param::Accumulator Accum, typename Function>
        requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>)
    auto executeBivariateWithCarryIn(uint32_t inst, Function&& func) -> void {
        executeBivariate<Accum, std::nullptr_t, std::nullptr_t, Param::CARRY_IN, Function>(inst, std::forward<Function>(func));
    }

  private:
    std::shared_ptr<Util::Logger> m_logger;
    CPU::Registers<Sys::RSP>*     m_gprs{};
    RSP::Registers*               m_vprs{};
    Memory::Memory*               m_memory{};

    CPU::InstructionExecutor<Sys::RSP> m_cpuExec;
};

template <Param::Accumulator Accum, typename VcoLoFunc, typename VcoHiFunc, Param::CarryIn Carry, typename Function>
    requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>> &&
             (std::same_as<std::nullptr_t, VcoLoFunc> || std::integral<std::invoke_result_t<VcoLoFunc, uint32_t>>) &&
             (std::same_as<std::nullptr_t, VcoHiFunc> || std::integral<std::invoke_result_t<VcoHiFunc, uint32_t>>))
auto InstructionExecutor::executeBivariate(uint32_t inst, Function&& func, VcoLoFunc vcoLoFunc, VcoHiFunc vcoHiFunc) -> void {
    const auto op    = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vsVpr = m_vprs->readVpr(op.vs);
    const auto vtVpr = m_vprs->readVpr(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));

    using VprType = std::conditional_t<Accum == Param::Accumulator::ACCUM_SIGN_EXT, int16_t, uint16_t>;
    auto result   = Registers::VPR<VprType>{};

    constexpr auto hasVcoLoFunc = !std::same_as<std::nullptr_t, VcoLoFunc>;
    constexpr auto hasVcoHiFunc = !std::same_as<std::nullptr_t, VcoHiFunc>;

    static_assert(Carry != Param::CarryIn::CARRY_IN || (!hasVcoLoFunc && !hasVcoHiFunc), "Cannot use carry-in with vco functions");

    auto vco = Registers::Control{};
    if constexpr (Carry == Param::CarryIn::CARRY_IN) {
        vco = m_vprs->readVco();
    }

    for (auto i = 0uz; i < 8; ++i) {
        auto value = func(vsVpr[i], vtVpr[i]);
        if constexpr (Carry == Param::CarryIn::CARRY_IN) {
            value = func(value, std::bit_cast<uint16_t>(vco));
        }
        result[i] = value;
        if constexpr (hasVcoLoFunc) {
            vco.low |= (vcoLoFunc(value) << i);
        }
        if constexpr (hasVcoHiFunc) {
            vco.high |= (vcoHiFunc(value) << i);
        }
    }

    if constexpr (hasVcoLoFunc || hasVcoHiFunc) {
        m_vprs->writeVco(vco);
    } else if constexpr (Carry == Param::CarryIn::CARRY_IN) {
        m_vprs->writeVco(Registers::Control{});
    }
    if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
        m_vprs->writeAccumulators(result);
    }
    m_vprs->writeVpr(op.vd, result);
}

// auto InstructionExecutor::executeAdd(uint32_t inst, Function&& func) -> void {
//     const auto op    = std::bit_cast<ISA::RSP::TypeVR>(inst);
//     const auto vsVpr = m_vprs->readVpr(op.vs);
//     const auto vtVpr = m_vprs->readVpr(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));
//
//     auto     result = RSP::Registers::VPR{};
//     uint16_t vco{};
//     for (auto i = 0uz; i < 8; ++i) {
//         uint32_t sum = vsVpr[i] + vtVpr[i];
//         vco |= (((sum >> 16) & 1) << i);
//         result[i] = sum;
//     }
//     m_vprs->writeVpr(op.vd, result);
//     m_vprs->writeVco(vco);
// }
} // namespace RSP