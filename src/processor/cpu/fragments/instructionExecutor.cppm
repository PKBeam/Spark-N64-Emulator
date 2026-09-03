module;

#include <util/defines.hpp>

export module CPU:InstructionExecutor;

import std;
import ISA;
import Memory;
import MemoryTypes;
import Util;

import :Registers;

constexpr auto RSP_DMEM_BASE = Util::rangeOf(Memory::PhysSeg::RSP_DMEM).lower;

export namespace Param {
// clang-format off
enum ShiftType        : bool    { LOGICAL, ARITHMETIC };
enum ShiftLen         : bool    { WORD, DOUBLE };
enum Direction        : bool    { LEFT, RIGHT };
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
    InstructionExecutor(std::shared_ptr<Util::Logger> logger,
                        CPU::Registers<System>*       regs,
                        Memory::MemoryBus*            memoryBus)
        : m_logger(logger),
          m_regs(regs),
          m_memoryBus(memoryBus) {}

    template <Param::BranchLink Link, Param::BranchSource Source>
    auto executeJump(uint32_t inst) -> void;

    template <Param::BranchLikelihood Likely = Param::BranchLikelihood::NOT_LIKELY>
    auto executeBranch(uint32_t inst, bool cond) -> void;

    template <Param::BranchLikelihood Likely = Param::BranchLikelihood::NOT_LIKELY, Param::BranchLink Link = Param::BranchLink::NO_LINK, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
    auto executeBranch(uint32_t inst, Function&& func) -> void;

    template <Param::BranchLikelihood Likely = Param::BranchLikelihood::NOT_LIKELY, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
    auto executeBranchAndLink(uint32_t inst, Function&& func) -> void {
        executeBranch<Likely, Param::BranchLink::LINK>(inst, std::forward<Function>(func));
    }

    template <Param::ShiftLen Len, Param::Direction Dir, Param::ShiftType Type, Param::ShiftVar Var = Param::ShiftVar::FIXED, Param::ShiftAdd Add = Param::ShiftAdd::ADD_NONE>
    auto executeShift(uint32_t inst) -> void;

    template <std::integral RegisterType = int32_t, Param::ImmediateExtend I = Param::ImmediateExtend::NO_IMM, typename Function>
        requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
    auto executeBivariate(uint32_t inst, Function&& func) -> void;

    template <Param::ImmediateExtend I = Param::ImmediateExtend::NO_IMM, std::integral RegisterType = int32_t, typename Function>
        requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
    auto executeBivariateImmediate(uint32_t inst, Function&& func) -> void {
        executeBivariate<RegisterType, I>(inst, std::forward<Function>(func));
    }

    template <Param::MemoryType Type, std::integral T>
    auto executeMemoryOperation(uint32_t inst) -> void;

    template <Param::MemoryType Type, Param::Direction Dir, std::integral T>
    auto executeMemoryOperationUnaligned(uint32_t inst) -> void;

    template <std::integral T>
    auto executeMultiply(uint32_t inst) -> void;

    template <std::integral T>
    auto executeDivide(uint32_t inst) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    CPU::Registers<System>*       m_regs{};
    Memory::MemoryBus*            m_memoryBus{};
};

template <Sys System>
template <Param::BranchLink Link, Param::BranchSource Source>
auto InstructionExecutor<System>::executeJump(uint32_t inst) -> void {
    if constexpr (Source == Param::BranchSource::IMM) {
        const auto ops = std::bit_cast<ISA::CPU::TypeJ>(inst);
        if constexpr (Link == Param::BranchLink::LINK) {
            m_regs->template writeGpr<ISA::CPU_REG::ra>(m_regs->readPc() + 8);
        }
        m_regs->writePcDelayed((m_regs->readPc() & 0xF0000000) | (ops.tgt << 2));
    } else {
        const auto ops = std::bit_cast<ISA::CPU::TypeR>(inst);
        if constexpr (Link == Param::BranchLink::LINK) {
            m_regs->template writeGpr(ops.rd, m_regs->readPc() + 8);
        }
        m_regs->writePcDelayed(m_regs->readGpr(ops.rs));
    }
}

template <Sys System>
template <Param::BranchLikelihood Likely>
auto InstructionExecutor<System>::executeBranch(uint32_t inst, bool cond) -> void {
    auto ops = std::bit_cast<ISA::CPU::TypeI>(inst);
    if (cond) {
        const auto instOffset = Util::signExt32<int16_t>(ops.imm);
        m_regs->writePcDelayed((m_regs->readPc() + 4) + (instOffset << 2));
    } else {
        if constexpr (Likely == Param::BranchLikelihood::LIKELY) {
            const auto nextPc = m_regs->readPc() + 4;
            m_regs->writePc(nextPc);
        }
    }
}

template <Sys System>
template <Param::BranchLikelihood Likely, Param::BranchLink Link, typename Function>
    requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
auto InstructionExecutor<System>::executeBranch(uint32_t inst, Function&& func) -> void {
    if constexpr (Link == Param::BranchLink::LINK) {
        m_regs->template writeGpr<ISA::CPU_REG::ra>(m_regs->readPc() + 8);
    }
    const auto ops       = std::bit_cast<ISA::CPU::TypeI>(inst);
    const bool condition = func(m_regs->readGpr(ops.rs), m_regs->readGpr(ops.rt));
    executeBranch<Likely>(inst, condition);
}

template <Sys System>
template <Param::ShiftLen Len, Param::Direction Dir, Param::ShiftType Type, Param::ShiftVar Var, Param::ShiftAdd Add>
auto InstructionExecutor<System>::executeShift(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::CPU::TypeR>(inst);

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
    if constexpr (Dir == Param::Direction::LEFT) {
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
    const auto ops = [&] {
        if constexpr (I == Param::ImmediateExtend::NO_IMM) {
            return std::bit_cast<ISA::CPU::TypeR>(inst);
        } else {
            return std::bit_cast<ISA::CPU::TypeI>(inst);
        }
    }();

    const auto arg1 = m_regs->template readGpr<RegisterType>(ops.rs);
    const auto arg2 = [&] {
        if constexpr (I == Param::ImmediateExtend::NO_IMM) {
            return m_regs->template readGpr<RegisterType>(ops.rt);
        } else if constexpr (I == Param::ImmediateExtend::SIGN_EXTEND) {
            return Util::signExt32<int16_t>(ops.imm);
        } else if constexpr (I == Param::ImmediateExtend::ZERO_EXTEND) {
            return ops.imm;
        }
    }();

    const auto dst = [&] {
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
    const auto ops    = std::bit_cast<ISA::CPU::TypeR>(inst);
    const auto rs     = static_cast<ResultType>(m_regs->template readGpr<T>(ops.rs));
    const auto rt     = static_cast<ResultType>(m_regs->template readGpr<T>(ops.rt));
    const auto result = rs * rt;
    m_regs->writeHi(static_cast<T>(result >> (sizeof(T) * 8)));
    m_regs->writeLo(static_cast<T>(result & ((static_cast<ResultType>(1) << (sizeof(T) * 8)) - 1)));
}

template <Sys System>
template <std::integral T>
auto InstructionExecutor<System>::executeDivide(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::CPU::TypeR>(inst);
    const auto rs  = m_regs->template readGpr<T>(ops.rs);
    const auto rt  = m_regs->template readGpr<T>(ops.rt);
    if (rt == 0) {
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::HIGH, Sev::WARNING, Sys::CPU>("division by zero @ PC " HEXFMT32, m_regs->readPc());
        }
        return;
    }
    if constexpr (std::is_signed_v<T>) {
        if (rs == std::numeric_limits<T>::min() && rt == -1) {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::CPU>("division overflow @ PC " HEXFMT32, m_regs->readPc());
            }
        }
    }
    m_regs->writeHi(static_cast<T>(rs % rt));
    m_regs->writeLo(static_cast<T>(rs / rt));
}

template <Sys System>
template <Param::MemoryType Type, std::integral T>
auto InstructionExecutor<System>::executeMemoryOperation(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::CPU::TypeI>(inst);

    auto addr = Util::signExt32<int16_t>(ops.imm) + m_regs->readGpr(ops.rs);
    if constexpr (System == Sys::RSP) {
        addr &= 0xFFF;
        addr += RSP_DMEM_BASE;
    }
    T data{};
    if constexpr (Type == Param::MemoryType::LOAD) {
        if constexpr (System == Sys::RSP) {
            data = m_memoryBus->readPhysical<T>(addr);
        } else {
            data = m_memoryBus->read<T>(addr);
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
            m_memoryBus->writePhysical<T>(addr, data);
        } else {
            m_memoryBus->write<T>(addr, data);
        }
    }
}

template <Sys System>
template <Param::MemoryType Type, Param::Direction Dir, std::integral T>
auto InstructionExecutor<System>::executeMemoryOperationUnaligned(uint32_t inst) -> void {
    const auto ops   = std::bit_cast<ISA::CPU::TypeI>(inst);
    const auto vaddr = Util::signExt32<int16_t>(ops.imm) + m_regs->readGpr(ops.rs);

    const auto addrRange = [vaddr]() {
        const auto base = static_cast<std::size_t>(vaddr);
        if constexpr (Dir == Param::Direction::LEFT) {
            // vaddr, vaddr + 1 ... next aligned address
            return std::views::iota(base, base + sizeof(T) - (base % sizeof(T)));
        } else {
            // vaddr, vaddr - 1 ... previous aligned address
            return std::views::iota(base - (base % sizeof(T)), base + 1) | std::views::reverse;
        }
    }();

    const auto byteRange = []() {
        constexpr auto range = std::views::iota(0uz, sizeof(T));
        if constexpr (Dir == Param::Direction::LEFT) {
            // access high bits first
            return range | std::views::reverse;
        } else {
            // access low bits first
            return range;
        }
    }();

    auto data = m_regs->template readGpr<T>(ops.rt);
    if constexpr (Type == Param::MemoryType::LOAD) {
        for (auto [byte, addr] : std::views::zip(byteRange, addrRange)) {
            const auto thisByte = m_memoryBus->read<uint8_t>(addr);
            data &= ~(static_cast<T>(0xFF) << (8 * byte));
            data |= (static_cast<T>(thisByte) << (8 * byte));
        }
        m_regs->template writeGpr<T>(ops.rt, data);
    } else {
        for (auto [byte, addr] : std::views::zip(byteRange, addrRange)) {
            const auto thisByte = (data >> (8 * byte)) & 0xFF;
            m_memoryBus->write<uint8_t>(addr, thisByte);
        }
    }
}
} // namespace CPU