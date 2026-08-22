module;

#include <util/defines.hpp>

module CPU;

import std;
import ISA;
import Memory;
import Util;
import :CPU;

namespace CPU {

template <Branch::Likelihood Likely, typename Function>
    requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
auto CPU::executeBranchAndLink(uint32_t inst, Function&& func) -> std::optional<uint64_t> {
    return executeBranch<Likely, Branch::LINK>(inst, std::forward<Function>(func));
}

template <Immediate::Extend I, std::integral RegisterType, typename Function>
    requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
auto CPU::executeBivariateImmediate(uint32_t inst, Function&& func) -> void {
    executeBivariate<RegisterType, I>(inst, std::forward<Function>(func));
}

template <Branch::Likelihood Likely, Branch::Link Link, typename Function>
    requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
auto CPU::executeBranch(uint32_t inst, Function&& func) -> std::optional<uint64_t> {
    if constexpr (Link == Branch::LINK) {
        writeGpr<ISA::CPU_REG::ra>(m_regs.pc + 8);
    }
    auto ops = std::bit_cast<ISA::CPU::TypeI>(inst);
    if (func(readGpr(ops.rs), readGpr(ops.rt))) {
        auto instOffset = Util::signExt32<int16_t>(ops.imm);
        if (instOffset == -1 &&                           // branches to previous instruction
            m_memory->read<uint32_t>(m_regs.pc - 4) == 0) // branches to NOP
        {
            throw Util::Error("Detected infinite loop @ PC {:#08x}: {:#08x}", m_regs.pc, inst);
        }
        return (m_regs.pc + 4) + (instOffset << 2);
    }
    if constexpr (Likely == Branch::LIKELY) {
        m_regs.pc += 4;
    }
    return std::nullopt;
}

template <Shift::Len Len, Shift::Dir Dir, Shift::Type Type, Shift::Var Var, Shift::Add Add>
auto CPU::executeShift(uint32_t inst) -> void {
    auto ops = std::bit_cast<ISA::CPU::TypeR>(inst);

    constexpr uint8_t rsMask      = Len == Shift::Len::WORD ? 0x1F : 0x3F;
    const uint8_t     fixedShift  = ops.sa + ((Add == Shift::Add::ADD32) ? 32 : 0);
    const uint8_t     shiftAmount = Var == Shift::Var::VARIABLE
                                        ? readGpr(ops.rs) & rsMask
                                        : fixedShift;

    constexpr auto T = [] consteval {
        constexpr auto regType = Len == Shift::Len::WORD
                                     ? ^^uint32_t
                                     : ^^uint64_t;

        return Type == Shift::Type::ARITHMETIC
                   ? std::meta::make_signed(regType)
                   : std::meta::make_unsigned(regType);
    }();

    auto value = readGpr<typename[:T:]>(ops.rt);
    if constexpr (Dir == Shift::Dir::LEFT) {
        value <<= shiftAmount;
    } else {
        value >>= shiftAmount;
    }
    writeGpr(ops.rd, value); // TODO sign extend value in 64-bit mode
}

template <std::integral RegisterType, Immediate::Extend I, typename Function>
    requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
auto CPU::executeBivariate(uint32_t inst, Function&& func) -> void {
    auto ops = [&] {
        if constexpr (I == Immediate::NONE) {
            return std::bit_cast<ISA::CPU::TypeR>(inst);
        } else {
            return std::bit_cast<ISA::CPU::TypeI>(inst);
        }
    }();

    auto arg1 = readGpr<RegisterType>(ops.rs);
    auto arg2 = [&] {
        if constexpr (I == Immediate::NONE) {
            return readGpr<RegisterType>(ops.rt);
        } else if constexpr (I == Immediate::SIGN_EXTEND) {
            return Util::signExt32<int16_t>(ops.imm);
        } else if constexpr (I == Immediate::ZERO_EXTEND) {
            return ops.imm;
        }
    }();

    auto dst = [&] {
        if constexpr (I == Immediate::NONE) {
            return ops.rd;
        } else {
            return ops.rt;
        }
    }();
    writeGpr(dst, func(arg1, arg2));
}

template <std::integral T>
auto CPU::executeMultiply(uint32_t inst) -> void {
#if defined(__SIZEOF_INT128__)
    using ResultType = std::conditional_t<sizeof(T) == 4, uint64_t, __uint128_t>;
#else
    static_assert(false, "No 128-bit integer available to implement DMULTU");
#endif
    auto ops    = std::bit_cast<ISA::CPU::TypeR>(inst);
    auto rs     = static_cast<ResultType>(readGpr<T>(ops.rs));
    auto rt     = static_cast<ResultType>(readGpr<T>(ops.rt));
    auto result = rs * rt;
    writeHi(static_cast<T>(result >> (sizeof(T) * 8)));
    writeLo(static_cast<T>(result & ((static_cast<ResultType>(1) << (sizeof(T) * 8)) - 1)));
}

template <std::integral T>
auto CPU::executeDivide(uint32_t inst) -> void {
    auto ops = std::bit_cast<ISA::CPU::TypeR>(inst);
    auto rs  = readGpr<T>(ops.rs);
    auto rt  = readGpr<T>(ops.rt);
    if (rt == 0) {
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Util::Verbosity::HIGH>("Warning: division by zero @ PC 0x{:08x}", m_regs.pc);
        }
        return;
    }
    if constexpr (std::is_signed_v<T>) {
        if (rs == std::numeric_limits<T>::min() && rt == -1) {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Util::Verbosity::HIGH>("Warning: division overflow @ PC 0x{:08x}", m_regs.pc);
            }
        }
    }
    writeHi(static_cast<T>(rs % rt));
    writeLo(static_cast<T>(rs / rt));
}

template <Memory::Type Type, std::integral T>
auto CPU::executeMemoryOperation(uint32_t inst) -> void {
    auto ops   = std::bit_cast<ISA::CPU::TypeI>(inst);
    auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
    if constexpr (Type == Memory::Type::LOAD) {
        auto result = m_memory->read<T>(vaddr);
        if constexpr (sizeof(T) == 8) {
            writeGpr(ops.rt, result);
        } else if constexpr (std::is_signed_v<T>) {
            // TODO 64-bit mode
            writeGpr(ops.rt, Util::signExt32(result));
        } else {
            writeGpr(ops.rt, static_cast<uint32_t>(result));
        }
    } else {
        m_memory->write<T>(vaddr, readGpr<T>(ops.rt));
    }
}

} // namespace CPU

#include "cpuExecuteInstruction.inl"
