module;

#include <util/defines.hpp>

export module CPU:VR4300;

import std;
import ISA;
import Memory;
import Util;

namespace Shift {
enum Type : bool {
    LOGICAL,
    ARITHMETIC,
};

enum Len : bool {
    WORD,
    DOUBLE,
};

enum Dir : bool {
    LEFT,
    RIGHT,
};

enum Add : bool {
    ADD_NONE,
    ADD32,
};

enum Var : bool {
    FIXED,
    VARIABLE,
};
} // namespace Shift

namespace Immediate {
enum Extend : uint8_t {
    NONE,
    SIGN_EXTEND,
    ZERO_EXTEND,
};
} // namespace Immediate

namespace Branch {
enum Likelihood : bool {
    NONE,
    LIKELY,
};
enum Link : bool {
    NO_LINK,
    LINK,
};
} // namespace Branch

namespace Memory {
enum Type : bool {
    LOAD,
    STORE,
};
} // namespace Memory

namespace Function {
static constexpr auto ADD     = [](auto a, auto b) { return a + b; };
static constexpr auto SUB     = [](auto a, auto b) { return a - b; };
static constexpr auto AND     = [](auto a, auto b) { return a & b; };
static constexpr auto OR      = [](auto a, auto b) { return a | b; };
static constexpr auto NOR     = [](auto a, auto b) { return ~(a | b); };
static constexpr auto XOR     = [](auto a, auto b) { return a ^ b; };
static constexpr auto CMP_EQ  = [](auto a, auto b) { return a == b; };
static constexpr auto CMP_NE  = [](auto a, auto b) { return a != b; };
static constexpr auto CMP_LE  = [](auto a, auto b) { return a <= b; };
static constexpr auto CMP_LT  = [](auto a, auto b) { return a < b; };
static constexpr auto CMP_GE  = [](auto a, auto b) { return a >= b; };
static constexpr auto CMP_NEZ = [](auto a, auto _) { return a != 0; };
static constexpr auto CMP_LEZ = [](auto a, auto _) { return a <= 0; };
static constexpr auto CMP_LTZ = [](auto a, auto _) { return a < 0; };
static constexpr auto CMP_GEZ = [](auto a, auto _) { return a >= 0; };
} // namespace Function

export class VR4300 {
  public:
    VR4300(
        std::shared_ptr<Util::Logger> logger,
        Memory::Memory*               memory);

    auto registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void;

    auto runInstruction() -> void;

    auto readPc() -> uint64_t;

    auto writePc(uint64_t value) -> void;

    template <std::integral T = int32_t>
    auto readGpr(std::size_t index) -> T;

    template <ISA::CPU_REG R, std::integral T = int32_t>
    auto readGpr() -> T;

    template <std::integral T>
    auto writeGpr(std::size_t index, T value) -> void;

    template <ISA::CPU_REG R, std::integral T>
    auto writeGpr(T value) -> void;

    template <std::integral T = int64_t>
    auto readHi() -> T;

    template <std::integral T>
    auto writeHi(T value) -> void;

    template <std::integral T = int64_t>
    auto readLo() -> T;

    template <std::integral T>
    auto writeLo(T value) -> void;

    template <std::integral T = int32_t>
    auto readCp0Reg(std::size_t index) -> T;

    template <ISA::CP0_REG R, std::integral T = int32_t>
    auto readCp0Reg() -> T;

    template <std::integral T>
    auto writeCp0Reg(std::size_t index, T value) -> void;

    template <ISA::CP0_REG R, std::integral T>
    auto writeCp0Reg(T value) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    Memory::Memory*               m_memory;
    std::optional<VirtualAddr>    m_delaySlotPc;

    struct { // do not write directly
        std::array<uint64_t, 32> gprs;
        std::array<double, 32>   fprs;
        uint64_t                 pc;
        uint64_t                 hi;
        uint64_t                 lo;
        bool                     llbit;
        float                    fcr0;
        float                    fcr31;
    } m_regs;

    std::function<void()>    m_bootCallback;
    uint32_t                 m_bootCallbackAddress;
    std::array<uint32_t, 32> m_cp0regs;

    // todo genericise to allow jump insts
    template <Branch::Likelihood Likely = Branch::NONE, Branch::Link Link = Branch::NO_LINK, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
    auto executeBranch(uint32_t inst, Function&& func) -> std::optional<uint64_t>;

    template <Branch::Likelihood Likely = Branch::NONE, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
    auto executeBranchAndLink(uint32_t inst, Function&& func) -> std::optional<uint64_t> {
        return executeBranch<Likely, Branch::LINK>(inst, std::forward<Function>(func));
    }

    template <Shift::Len Len, Shift::Dir Dir, Shift::Type Type, Shift::Var Var = Shift::Var::FIXED, Shift::Add Add = Shift::Add::ADD_NONE>
    auto executeShift(uint32_t inst) -> void;

    template <std::integral RegisterType = int32_t, Immediate::Extend I = Immediate::NONE, typename Function>
        requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
    auto executeBivariate(uint32_t inst, Function&& func) -> void;

    template <Immediate::Extend I = Immediate::NONE, std::integral RegisterType = int32_t, typename Function>
        requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
    auto executeBivariateImmediate(uint32_t inst, Function&& func) -> void {
        executeBivariate<RegisterType, I>(inst, std::forward<Function>(func));
    }

    template <Memory::Type Type, std::integral T>
    auto executeMemoryOperation(uint32_t inst) -> void;

    template <std::integral T>
    auto executeMultiply(uint32_t inst) -> void;

    template <std::integral T>
    auto executeDivide(uint32_t inst) -> void;
};

struct CP0_STATUS {
    uint32_t ie  : 1;
    uint32_t exl : 1;
    uint32_t erl : 1;
    uint32_t ksu : 1;
    uint32_t ux  : 1;
    uint32_t sx  : 1;
    uint32_t kx  : 1;
    uint32_t im  : 8;
    uint32_t ds  : 9;
    uint32_t re  : 1;
    uint32_t fr  : 1;
    uint32_t rp  : 1;
    uint32_t cu  : 4;
};

VR4300::VR4300(std::shared_ptr<Util::Logger> logger, Memory::Memory* memory) {
    m_memory    = memory;
    m_logger    = logger;
    m_regs.gprs = {};
}

auto VR4300::registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void {
    m_bootCallbackAddress = callbackBootAddress;
    m_bootCallback        = std::move(callback);
}

auto VR4300::runInstruction() -> void {
    using namespace Opcodes;
    using namespace ISA;

    auto instBits = WITH_LOG_DISABLED(m_logger, m_memory->read<uint32_t>(m_regs.pc));

    auto inst = ISA::Instruction(instBits);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"pc", "0x{:08x}", static_cast<uint32_t>(m_regs.pc)},
            std::tuple{"inst", "{}", inst});
    }

    std::optional<uint64_t> pcJumpValue = std::nullopt;

    auto op   = inst.opcode;
    auto data = inst.data;

    auto checkBranch = [&, this](auto&& comp, bool likely = false) {
        auto ops = std::bit_cast<CPU::TypeI>(data);
        if (comp(readGpr(ops.rs), readGpr(ops.rt))) {
            pcJumpValue = (m_regs.pc + 4) + (Util::signExt32<int16_t>(ops.imm) << 2);
        } else if (likely) {
            m_regs.pc += 4;
        }
    };

    switch (op) {
        case UnifiedOpcode::OP_NOP:
            break;

        // jump instructions
        case UnifiedOpcode::OP_J: {
            auto ops    = std::bit_cast<CPU::TypeJ>(data);
            pcJumpValue = (m_regs.pc & 0xF0000000) | (ops.tgt << 2);
            break;
        }
        case UnifiedOpcode::OP_JAL: {
            auto ops    = std::bit_cast<CPU::TypeJ>(data);
            pcJumpValue = (m_regs.pc & 0xF0000000) | (ops.tgt << 2);
            writeGpr<CPU_REG::ra>(m_regs.pc + 8);
            break;
        }
        case UnifiedOpcode::OP_JR: {
            auto ops    = std::bit_cast<CPU::TypeR>(data);
            pcJumpValue = readGpr(ops.rs);
            if (static_cast<uint32_t>(*pcJumpValue) == m_bootCallbackAddress) {
                m_bootCallback();
            }
            break;
        }
        case UnifiedOpcode::OP_JALR: {
            auto ops    = std::bit_cast<CPU::TypeR>(data);
            pcJumpValue = readGpr(ops.rs);
            writeGpr(ops.rd, m_regs.pc + 8);
            break;
        }

        // branch instructions
        case UnifiedOpcode::OP_BNE: pcJumpValue = executeBranch(data, Function::CMP_NE); break;
        case UnifiedOpcode::OP_BNEL: pcJumpValue = executeBranch<Branch::LIKELY>(data, Function::CMP_NE); break;
        case UnifiedOpcode::OP_BLTZ: pcJumpValue = executeBranch(data, Function::CMP_LTZ); break;
        case UnifiedOpcode::OP_BLTZAL: pcJumpValue = executeBranchAndLink(data, Function::CMP_LTZ); break;
        case UnifiedOpcode::OP_BLEZ: pcJumpValue = executeBranch(data, Function::CMP_LEZ); break;
        case UnifiedOpcode::OP_BLEZL: pcJumpValue = executeBranch<Branch::LIKELY>(data, Function::CMP_LEZ); break;
        case UnifiedOpcode::OP_BGEZ: pcJumpValue = executeBranch(data, Function::CMP_GEZ); break;
        case UnifiedOpcode::OP_BGEZL: pcJumpValue = executeBranch<Branch::LIKELY>(data, Function::CMP_GEZ); break;
        case UnifiedOpcode::OP_BGEZAL: pcJumpValue = executeBranchAndLink(data, Function::CMP_GEZ); break;
        case UnifiedOpcode::OP_BEQ: pcJumpValue = executeBranch(data, Function::CMP_EQ); break;
        case UnifiedOpcode::OP_BEQL: pcJumpValue = executeBranch<Branch::LIKELY>(data, Function::CMP_EQ); break;

        // load/store instructions
        case UnifiedOpcode::OP_LUI: {
            auto ops = std::bit_cast<CPU::TypeI>(data);
            writeGpr(ops.rt, Util::signExt32(ops.imm << 16));
            break;
        }
        case UnifiedOpcode::OP_LB: executeMemoryOperation<Memory::LOAD, int8_t>(data); break;
        case UnifiedOpcode::OP_LBU: executeMemoryOperation<Memory::LOAD, uint8_t>(data); break;
        case UnifiedOpcode::OP_LH: executeMemoryOperation<Memory::LOAD, int16_t>(data); break;
        case UnifiedOpcode::OP_LHU: executeMemoryOperation<Memory::LOAD, uint16_t>(data); break;
        case UnifiedOpcode::OP_LW: executeMemoryOperation<Memory::LOAD, int32_t>(data); break;
        case UnifiedOpcode::OP_LWU: executeMemoryOperation<Memory::LOAD, uint32_t>(data); break;
        case UnifiedOpcode::OP_LD: executeMemoryOperation<Memory::LOAD, int64_t>(data); break;
        case UnifiedOpcode::OP_SB: executeMemoryOperation<Memory::STORE, int8_t>(data); break;
        case UnifiedOpcode::OP_SH: executeMemoryOperation<Memory::STORE, int16_t>(data); break;
        case UnifiedOpcode::OP_SW: executeMemoryOperation<Memory::STORE, int32_t>(data); break;
        case UnifiedOpcode::OP_SD: executeMemoryOperation<Memory::STORE, int64_t>(data); break;

        case UnifiedOpcode::OP_SWL: {
            auto ops   = std::bit_cast<CPU::TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
            for (auto byte = 4z; byte > vaddr % 4; --byte) {
                auto thisByte = (readGpr<uint32_t>(ops.rt) >> (8 * byte)) & 0xFF;
                // TODO make this more HW accurate
                m_memory->write<uint8_t>(vaddr + (4 - byte), thisByte);
            }
            break;
        }

        // multiply/divide ops
        case UnifiedOpcode::OP_MULT: executeMultiply<int32_t>(data); break;
        case UnifiedOpcode::OP_MULTU: executeMultiply<uint32_t>(data); break;
        case UnifiedOpcode::OP_DMULT: executeMultiply<int64_t>(data); break;
        case UnifiedOpcode::OP_DMULTU: executeMultiply<uint64_t>(data); break;

        case UnifiedOpcode::OP_DIV: executeDivide<int32_t>(data); break;
        case UnifiedOpcode::OP_DIVU: executeDivide<uint32_t>(data); break;
        case UnifiedOpcode::OP_DDIV: executeDivide<int64_t>(data); break;
        case UnifiedOpcode::OP_DDIVU: executeDivide<uint64_t>(data); break;

        case UnifiedOpcode::OP_MFLO: writeGpr(std::bit_cast<CPU::TypeR>(data).rd, readLo()); break;
        case UnifiedOpcode::OP_MFHI: writeGpr(std::bit_cast<CPU::TypeR>(data).rd, readHi()); break;
        case UnifiedOpcode::OP_MTLO: writeLo(readGpr(std::bit_cast<CPU::TypeR>(data).rs)); break;
        case UnifiedOpcode::OP_MTHI: writeHi(readGpr(std::bit_cast<CPU::TypeR>(data).rs)); break;

        // other arithmetic ops
        case UnifiedOpcode::OP_ADD: [[fallthrough]]; // TODO exception on overflow for ADDI
        case UnifiedOpcode::OP_ADDU: executeBivariate(data, Function::ADD); break;
        case UnifiedOpcode::OP_ADDI: [[fallthrough]]; // TODO exception on overflow for ADDI
        case UnifiedOpcode::OP_ADDIU: executeBivariateImmediate<Immediate::SIGN_EXTEND>(data, Function::ADD); break;

        case UnifiedOpcode::OP_SLT: executeBivariate(data, Function::CMP_LT); break;
        case UnifiedOpcode::OP_SLTU: executeBivariate<uint32_t>(data, Function::CMP_LT); break;
        case UnifiedOpcode::OP_SLTI: executeBivariateImmediate<Immediate::SIGN_EXTEND, int32_t>(data, Function::CMP_LT); break;
        case UnifiedOpcode::OP_SLTIU: executeBivariateImmediate<Immediate::ZERO_EXTEND, uint32_t>(data, Function::CMP_LT); break;

        case UnifiedOpcode::OP_SUBU: executeBivariate(data, Function::SUB); break;

        case UnifiedOpcode::OP_AND: executeBivariate(data, Function::AND); break;
        case UnifiedOpcode::OP_ANDI: executeBivariateImmediate<Immediate::ZERO_EXTEND>(data, Function::AND); break;
        case UnifiedOpcode::OP_OR: executeBivariate(data, Function::OR); break;
        case UnifiedOpcode::OP_ORI: executeBivariateImmediate<Immediate::ZERO_EXTEND>(data, Function::OR); break;
        case UnifiedOpcode::OP_XOR: executeBivariate(data, Function::XOR); break;
        case UnifiedOpcode::OP_XORI: executeBivariateImmediate<Immediate::ZERO_EXTEND>(data, Function::XOR); break;
        case UnifiedOpcode::OP_NOR: executeBivariate(data, Function::NOR); break;

        // shift instructions
        case UnifiedOpcode::OP_SLL: executeShift<Shift::WORD, Shift::LEFT, Shift::LOGICAL>(data); break;
        case UnifiedOpcode::OP_SRL: executeShift<Shift::WORD, Shift::RIGHT, Shift::LOGICAL>(data); break;
        case UnifiedOpcode::OP_SRA: executeShift<Shift::WORD, Shift::RIGHT, Shift::ARITHMETIC>(data); break;
        case UnifiedOpcode::OP_SLLV: executeShift<Shift::WORD, Shift::LEFT, Shift::LOGICAL, Shift::VARIABLE>(data); break;
        case UnifiedOpcode::OP_SRLV: executeShift<Shift::WORD, Shift::RIGHT, Shift::LOGICAL, Shift::VARIABLE>(data); break;
        case UnifiedOpcode::OP_SRAV: executeShift<Shift::WORD, Shift::RIGHT, Shift::ARITHMETIC, Shift::VARIABLE>(data); break;
        case UnifiedOpcode::OP_DSLL: executeShift<Shift::DOUBLE, Shift::LEFT, Shift::LOGICAL>(data); break;
        case UnifiedOpcode::OP_DSRL: executeShift<Shift::DOUBLE, Shift::RIGHT, Shift::LOGICAL>(data); break;
        case UnifiedOpcode::OP_DSRA: executeShift<Shift::DOUBLE, Shift::RIGHT, Shift::ARITHMETIC>(data); break;
        case UnifiedOpcode::OP_DSLLV: executeShift<Shift::DOUBLE, Shift::LEFT, Shift::LOGICAL, Shift::VARIABLE>(data); break;
        case UnifiedOpcode::OP_DSRLV: executeShift<Shift::DOUBLE, Shift::RIGHT, Shift::LOGICAL, Shift::VARIABLE>(data); break;
        case UnifiedOpcode::OP_DSRAV: executeShift<Shift::DOUBLE, Shift::RIGHT, Shift::ARITHMETIC, Shift::VARIABLE>(data); break;
        case UnifiedOpcode::OP_DSLL32: executeShift<Shift::DOUBLE, Shift::LEFT, Shift::LOGICAL, Shift::FIXED, Shift::ADD32>(data); break;
        case UnifiedOpcode::OP_DSRL32: executeShift<Shift::DOUBLE, Shift::RIGHT, Shift::LOGICAL, Shift::FIXED, Shift::ADD32>(data); break;
        case UnifiedOpcode::OP_DSRA32: executeShift<Shift::DOUBLE, Shift::RIGHT, Shift::ARITHMETIC, Shift::FIXED, Shift::ADD32>(data); break;

        case UnifiedOpcode::OP_MFCz: {
            auto cp = (data >> 26) & 0b11;
            if (cp != 0) throw Util::Error("Unsupported instruction on coprocessor {}", cp);

            auto ops = std::bit_cast<CPU::TypeR>(data);
            writeGpr(ops.rt, readCp0Reg(ops.rd));
            break;
        }
        case UnifiedOpcode::OP_MTCz: {
            auto cp = (data >> 26) & 0b11;
            if (cp != 0) throw Util::Error("Unsupported instruction on coprocessor {}", cp);

            auto ops = std::bit_cast<CPU::TypeR>(data);
            writeCp0Reg(ops.rd, readGpr(ops.rt));
            break;
        }
        case UnifiedOpcode::OP_CFCz: {
            auto cp  = (data >> 26) & 0b11;
            auto ops = std::bit_cast<CPU::TypeR>(data);
            if (cp == 1 && ops.rd == 31) {
                writeGpr(ops.rt, 0);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Util::Verbosity::HIGH>("Warning: ignored read from FCR31");
                }
                break;
            }
            throw Util::Error("Unsupported instruction on coprocessor {}", cp);
            break;
        }
        case UnifiedOpcode::OP_CTCz: {
            auto cp  = (data >> 26) & 0b11;
            auto ops = std::bit_cast<CPU::TypeR>(data);
            if (cp == 1 && ops.rd == 31) {
                auto value = readGpr(ops.rt);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Util::Verbosity::HIGH>("Warning: ignored write of {:#08x} to FCR31", value);
                }
                break;
            }
            throw Util::Error("Unsupported instruction on coprocessor {}", cp);
            break;
        }

        // TODO implement these
        case UnifiedOpcode::OP_TLBR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWI: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBP: {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Util::Verbosity::HIGH>("Warning: ignored TLB instruction {}", inst);
            }
            break;
        }
        case UnifiedOpcode::OP_CACHE: {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"warning", "skipped a CACHE instruction"},
                    std::tuple{"data", "{}", inst.data});
            }
            break;
        }
        case UnifiedOpcode::OP_ERET: {
            auto status = std::bit_cast<CP0_STATUS>(readCp0Reg<ISA::CP0_REG::STATUS>());
            if (status.erl) {
                writePc(readCp0Reg<ISA::CP0_REG::ERROREPC>());
                status.erl = 0;
            } else {
                writePc(readCp0Reg<ISA::CP0_REG::EPC>());
                status.exl = 0;
            }
            writeCp0Reg<ISA::CP0_REG::STATUS>(std::bit_cast<uint32_t>(status));
            break;
        }
        default: {
            throw Util::Error("Unimplemented instruction @ PC 0x{:08x}: {}", m_regs.pc, inst);
        }
    }

    // detect boot checksum fail
    if (op == UnifiedOpcode::OP_BGEZAL) {
        auto ops = std::bit_cast<CPU::TypeI>(data);
        if (ops.rs == static_cast<uint32_t>(CPU_REG::zero) && static_cast<int16_t>(ops.imm) == -1) {
            if (m_memory->read<uint32_t>(m_regs.pc - 4) == 0) {
                throw Util::Error("Detected infinite looping BGEZAL->NOP, likely boot checksum fail.");
            }
        }
    }

    if (m_delaySlotPc) {
        writePc(*m_delaySlotPc);
        m_delaySlotPc.reset();
    } else {
        m_regs.pc += 4;
    }

    if (pcJumpValue) {
        m_delaySlotPc = pcJumpValue;
    }
}

template <Branch::Likelihood Likely, Branch::Link Link, typename Function>
    requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
auto VR4300::executeBranch(uint32_t inst, Function&& func) -> std::optional<uint64_t> {
    if constexpr (Link == Branch::LINK) {
        writeGpr<ISA::CPU_REG::ra>(m_regs.pc + 8);
    }
    auto ops = std::bit_cast<ISA::CPU::TypeI>(inst);
    if (func(readGpr(ops.rs), readGpr(ops.rt))) {
        return (m_regs.pc + 4) + (Util::signExt32<int16_t>(ops.imm) << 2);
    }
    if constexpr (Likely == Branch::LIKELY) {
        m_regs.pc += 4;
    }
    return std::nullopt;
}

template <Shift::Len Len, Shift::Dir Dir, Shift::Type Type, Shift::Var Var, Shift::Add Add>
auto VR4300::executeShift(uint32_t inst) -> void {
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
auto VR4300::executeBivariate(uint32_t inst, Function&& func) -> void {
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
auto VR4300::executeMultiply(uint32_t inst) -> void {
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
auto VR4300::executeDivide(uint32_t inst) -> void {
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
auto VR4300::executeMemoryOperation(uint32_t inst) -> void {
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

auto VR4300::readPc() -> uint64_t {
    auto value = m_regs.pc;
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MAX>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "PC"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

auto VR4300::writePc(uint64_t value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MAX>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "PC"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    m_regs.pc = value;
}

template <std::integral T>
auto VR4300::readGpr(std::size_t index) -> T {
    auto value = static_cast<T>(m_regs.gprs[index]);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "{}", static_cast<ISA::CPU_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return index == 0 ? 0 : value;
}

template <ISA::CPU_REG R, std::integral T>
auto VR4300::readGpr() -> T {
    return readGpr<T>(static_cast<std::size_t>(R));
}

template <std::integral T>
auto VR4300::writeGpr(std::size_t index, T value) -> void {
    if (index != 0) {
        m_regs.gprs[index] = value;
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Util::Verbosity::HIGH>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "{}", static_cast<ISA::CPU_REG>(index)},
                std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
        }
    }
}

template <ISA::CPU_REG R, std::integral T>
auto VR4300::writeGpr(T value) -> void {
    writeGpr<T>(static_cast<std::size_t>(R), value);
}

template <std::integral T>
auto VR4300::readHi() -> T {
    auto value = static_cast<T>(m_regs.hi);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <std::integral T>
auto VR4300::writeHi(T value) -> void {
    m_regs.hi = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <std::integral T>
auto VR4300::readLo() -> T {
    auto value = static_cast<T>(m_regs.lo);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <std::integral T>
auto VR4300::writeLo(T value) -> void {
    m_regs.lo = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <std::integral T>
auto VR4300::readCp0Reg(std::size_t index) -> T {
    const auto regName = static_cast<ISA::CP0_REG>(index);

    auto value = static_cast<T>(m_cp0regs[index]);
    IF_LOG_ENABLED(m_logger) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "CP0 {}", static_cast<ISA::CP0_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <ISA::CP0_REG R, std::integral T>
auto VR4300::readCp0Reg() -> T {
    return readCp0Reg(static_cast<std::size_t>(R));
}

template <std::integral T>
auto VR4300::writeCp0Reg(std::size_t index, T value) -> void {
    const auto regName = static_cast<ISA::CP0_REG>(index);
    if (m_logger && regName == ISA::CP0_REG::RANDOM) {
        m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Attempted to write to read-only register CP0_REG::RANDOM!"});
        return;
    }
    if (m_logger && regName == ISA::CP0_REG::STATUS) {
        auto status = std::bit_cast<CP0_STATUS>(static_cast<uint32_t>(value));
        if (status.kx || status.sx || status.ux) {
            m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Enabled 64-bit mode in CP0_REG::STATUS, which is not fully supported yet"});
        }
    }
    m_cp0regs[index] = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "CP0 {}", static_cast<ISA::CP0_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <ISA::CP0_REG R, std::integral T>
auto VR4300::writeCp0Reg(T value) -> void {
    writeCp0Reg(static_cast<std::size_t>(R), value);
}
