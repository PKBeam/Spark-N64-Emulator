module;

#include <util/defines.hpp>

export module CPU:InstructionExecutor;

import std;
import ISA;
import Memory;
import Util;

import :Registers;

constexpr auto RSP_DMEM_BASE = Memory::rangeOf(Memory::PhysSeg::RSP_DMEM).lower;

export namespace Param {
// clang-format off
enum ShiftType        : bool    { LOGICAL, ARITHMETIC };
enum ShiftLen         : bool    { WORD, DOUBLE };
enum ShiftDir         : bool    { LEFT, RIGHT };
enum ShiftAdd         : bool    { ADD_NONE, ADD32 };
enum ShiftVar         : bool    { FIXED, VARIABLE };
enum ImmediateExtend  : uint8_t { NO_IMM, SIGN_EXTEND, ZERO_EXTEND };
enum BranchLikelihood : bool    { NOT_LIKELY, LIKELY };
enum BranchLink       : bool    { NO_LINK, LINK };
enum BranchSource     : bool    { IMM, REG };
enum MemoryType       : bool    { LOAD, STORE };
// clang-format on
} // namespace Param

export namespace CPU {

template <Sys System>
class InstructionExecutor {
  public:
    InstructionExecutor(std::shared_ptr<Util::Logger> logger, CPU::Registers<System>* regs, Memory::Memory* memory)
        : m_logger(logger), m_regs(regs), m_memory(memory) {}

    template <Param::BranchLink Link, Param::BranchSource Source>
    auto executeJump(uint32_t inst) -> void;

    template <Param::BranchLikelihood Likely = Param::BranchLikelihood::NOT_LIKELY, Param::BranchLink Link = Param::BranchLink::NO_LINK, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
    auto executeBranch(uint32_t inst, Function&& func) -> void;

    template <Param::BranchLikelihood Likely = Param::BranchLikelihood::NOT_LIKELY, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
    auto executeBranchAndLink(uint32_t inst, Function&& func) -> void;

    template <Param::ShiftLen Len, Param::ShiftDir Dir, Param::ShiftType Type, Param::ShiftVar Var = Param::ShiftVar::FIXED, Param::ShiftAdd Add = Param::ShiftAdd::ADD_NONE>
    auto executeShift(uint32_t inst) -> void;

    template <std::integral RegisterType = int32_t, Param::ImmediateExtend I = Param::ImmediateExtend::NO_IMM, typename Function>
        requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
    auto executeBivariate(uint32_t inst, Function&& func) -> void;

    template <Param::ImmediateExtend I = Param::ImmediateExtend::NO_IMM, std::integral RegisterType = int32_t, typename Function>
        requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
    auto executeBivariateImmediate(uint32_t inst, Function&& func) -> void;

    template <Param::MemoryType Type, std::integral T>
    auto executeMemoryOperation(uint32_t inst) -> void;

    template <std::integral T>
    auto executeMultiply(uint32_t inst) -> void;

    template <std::integral T>
    auto executeDivide(uint32_t inst) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    CPU::Registers<System>*       m_regs{};
    Memory::Memory*               m_memory{};
};

template <Sys System>
template <Param::BranchLink Link, Param::BranchSource Source>
auto InstructionExecutor<System>::executeJump(uint32_t inst) -> void {
    if constexpr (Source == Param::BranchSource::IMM) {
        auto ops = std::bit_cast<ISA::CPU::TypeJ>(inst);
        if constexpr (Link == Param::BranchLink::LINK) {
            m_regs->template writeGpr<ISA::CPU_REG::ra>(m_regs->readPc() + 8);
        }
        m_regs->writePcDelayed((m_regs->readPc() & 0xF0000000) | (ops.tgt << 2));
    } else {
        auto ops = std::bit_cast<ISA::CPU::TypeR>(inst);
        if constexpr (Link == Param::BranchLink::LINK) {
            m_regs->template writeGpr(ops.rd, m_regs->readPc() + 8);
        }
        m_regs->writePcDelayed(m_regs->readGpr(ops.rs));
    }
}

template <Sys System>
template <Param::BranchLikelihood Likely, typename Function>
    requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
auto InstructionExecutor<System>::executeBranchAndLink(uint32_t inst, Function&& func) -> void {
    executeBranch<Likely, Param::BranchLink::LINK>(inst, std::forward<Function>(func));
}

template <Sys System>
template <Param::ImmediateExtend I, std::integral RegisterType, typename Function>
    requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
auto InstructionExecutor<System>::executeBivariateImmediate(uint32_t inst, Function&& func) -> void {
    executeBivariate<RegisterType, I>(inst, std::forward<Function>(func));
}

template <Sys System>
template <Param::BranchLikelihood Likely, Param::BranchLink Link, typename Function>
    requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
auto InstructionExecutor<System>::executeBranch(uint32_t inst, Function&& func) -> void {
    if constexpr (Link == Param::BranchLink::LINK) {
        m_regs->template writeGpr<ISA::CPU_REG::ra>(m_regs->readPc() + 8);
    }
    auto ops = std::bit_cast<ISA::CPU::TypeI>(inst);
    if (func(m_regs->readGpr(ops.rs), m_regs->readGpr(ops.rt))) {
        auto instOffset = Util::signExt32<int16_t>(ops.imm);

        // auto prevInst = [this] {
        //     const auto prevPc = m_regs->readPc() - 4;
        //     if constexpr (System == Sys::RSP) {
        //         return m_memory->readPhysical<uint32_t>(RSP_DMEM_BASE + prevPc);
        //     } else {
        //         return m_memory->read<uint32_t>(prevPc);
        //     }
        // }();
        // if (instOffset == -1 && // branches to previous instruction
        //     prevInst == 0)      // branches to NOP
        //{
        //     throw Util::Error("Detected infinite loop @ PC {:#08x}: {:#08x}", m_regs->readPc(), inst);
        // }
        m_regs->writePcDelayed((m_regs->readPc() + 4) + (instOffset << 2));
    } else {
        if constexpr (Likely == Param::BranchLikelihood::LIKELY) {
            auto nextPc = m_regs->readPc() + 4;
            m_regs->writePc(nextPc);
        }
    }
}

template <Sys System>
template <Param::ShiftLen Len, Param::ShiftDir Dir, Param::ShiftType Type, Param::ShiftVar Var, Param::ShiftAdd Add>
auto InstructionExecutor<System>::executeShift(uint32_t inst) -> void {
    auto ops = std::bit_cast<ISA::CPU::TypeR>(inst);

    constexpr uint8_t rsMask      = Len == Param::ShiftLen::WORD ? 0x1F : 0x3F;
    const uint8_t     fixedShift  = ops.sa + ((Add == Param::ShiftAdd::ADD32) ? 32 : 0);
    const uint8_t     shiftAmount = Var == Param::ShiftVar::VARIABLE
                                        ? m_regs->readGpr(ops.rs) & rsMask
                                        : fixedShift;

    constexpr auto T = [] consteval {
        constexpr auto regType = Len == Param::ShiftLen::WORD
                                     ? ^^uint32_t
                                     : ^^uint64_t;

        return Type == Param::ShiftType::ARITHMETIC
                   ? std::meta::make_signed(regType)
                   : std::meta::make_unsigned(regType);
    }();

    auto value = m_regs->template readGpr<typename[:T:]>(ops.rt);
    if constexpr (Dir == Param::ShiftDir::LEFT) {
        value <<= shiftAmount;
    } else {
        value >>= shiftAmount;
    }
    m_regs->writeGpr(ops.rd, value); // TODO sign extend value in 64-bit mode
}

template <Sys System>
template <std::integral RegisterType, Param::ImmediateExtend I, typename Function>
    requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
auto InstructionExecutor<System>::executeBivariate(uint32_t inst, Function&& func) -> void {
    auto ops = [&] {
        if constexpr (I == Param::ImmediateExtend::NO_IMM) {
            return std::bit_cast<ISA::CPU::TypeR>(inst);
        } else {
            return std::bit_cast<ISA::CPU::TypeI>(inst);
        }
    }();

    auto arg1 = m_regs->template readGpr<RegisterType>(ops.rs);
    auto arg2 = [&] {
        if constexpr (I == Param::ImmediateExtend::NO_IMM) {
            return m_regs->template readGpr<RegisterType>(ops.rt);
        } else if constexpr (I == Param::ImmediateExtend::SIGN_EXTEND) {
            return Util::signExt32<int16_t>(ops.imm);
        } else if constexpr (I == Param::ImmediateExtend::ZERO_EXTEND) {
            return ops.imm;
        }
    }();

    auto dst = [&] {
        if constexpr (I == Param::ImmediateExtend::NO_IMM) {
            return ops.rd;
        } else {
            return ops.rt;
        }
    }();
    m_regs->writeGpr(dst, func(arg1, arg2));
}

template <Sys System>
template <std::integral T>
auto InstructionExecutor<System>::executeMultiply(uint32_t inst) -> void {
#if defined(__SIZEOF_INT128__)
    using ResultType = std::conditional_t<sizeof(T) == 4, uint64_t, __uint128_t>;
#else
    static_assert(false, "No 128-bit integer available to implement DMULTU");
#endif
    auto ops    = std::bit_cast<ISA::CPU::TypeR>(inst);
    auto rs     = static_cast<ResultType>(m_regs->template readGpr<T>(ops.rs));
    auto rt     = static_cast<ResultType>(m_regs->template readGpr<T>(ops.rt));
    auto result = rs * rt;
    m_regs->writeHi(static_cast<T>(result >> (sizeof(T) * 8)));
    m_regs->writeLo(static_cast<T>(result & ((static_cast<ResultType>(1) << (sizeof(T) * 8)) - 1)));
}

template <Sys System>
template <std::integral T>
auto InstructionExecutor<System>::executeDivide(uint32_t inst) -> void {
    auto ops = std::bit_cast<ISA::CPU::TypeR>(inst);
    auto rs  = m_regs->template readGpr<T>(ops.rs);
    auto rt  = m_regs->template readGpr<T>(ops.rt);
    if (rt == 0) {
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::HIGH, Sev::WARNING, Sys::CPU>("division by zero @ PC 0x{:08x}", m_regs->readPc());
        }
        return;
    }
    if constexpr (std::is_signed_v<T>) {
        if (rs == std::numeric_limits<T>::min() && rt == -1) {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::CPU>("division overflow @ PC 0x{:08x}", m_regs->readPc());
            }
        }
    }
    m_regs->writeHi(static_cast<T>(rs % rt));
    m_regs->writeLo(static_cast<T>(rs / rt));
}

template <Sys System>
template <Param::MemoryType Type, std::integral T>
auto InstructionExecutor<System>::executeMemoryOperation(uint32_t inst) -> void {
    auto ops  = std::bit_cast<ISA::CPU::TypeI>(inst);
    auto addr = Util::signExt32<int16_t>(ops.imm) + m_regs->readGpr(ops.rs);
    if constexpr (System == Sys::RSP) {
        addr &= 0xFFF;
        addr += RSP_DMEM_BASE;
    }
    T data{};
    if constexpr (Type == Param::MemoryType::LOAD) {
        if constexpr (System == Sys::RSP) {
            data = m_memory->readPhysical<T>(addr);
        } else {
            data = m_memory->read<T>(addr);
        }
        if constexpr (sizeof(T) == 8) {
            m_regs->writeGpr(ops.rt, data);
        } else if constexpr (std::is_signed_v<T>) {
            // TODO 64-bit mode
            m_regs->writeGpr(ops.rt, Util::signExt32(data));
        } else {
            m_regs->writeGpr(ops.rt, static_cast<uint32_t>(data));
        }
    } else {
        data = m_regs->template readGpr<T>(ops.rt);
        if constexpr (System == Sys::RSP) {
            m_memory->writePhysical<T>(addr, data);
        } else {
            m_memory->write<T>(addr, data);
        }
    }
}
} // namespace CPU