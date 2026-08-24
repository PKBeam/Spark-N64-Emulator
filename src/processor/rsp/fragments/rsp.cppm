module;

#include <util/defines.hpp>

export module RSP:RSP;

import std;
import CP0;
import CPU;
import ISA;
import Memory;
import RspControl;
import Util;

import :Registers;

using Control = RSP::Control;

constexpr auto RSP_IMEM_BASE = Memory::rangeOf(Memory::PhysSeg::RSP_IMEM).lower;

export namespace RSP {

class RSP {
  public:
    RSP(std::shared_ptr<Util::Logger> logger, Memory::Memory* memory, Control* control)
        : m_logger(logger), m_memory(memory), m_sRegs(logger), m_vRegs(logger), m_control(control), m_exec(m_logger, &m_sRegs, m_memory) {};

    auto runInstruction() -> void;

    auto halt() -> void;

  private:
    std::shared_ptr<Util::Logger>      m_logger;
    Memory::Memory*                    m_memory{};
    CPU::Registers<Sys::RSP>           m_sRegs;
    ::RSP::Registers                   m_vRegs;
    Control*                           m_control{};
    CPU::InstructionExecutor<Sys::RSP> m_exec;
    std::optional<VirtualAddr>         m_delaySlotPc;
};

auto RSP::runInstruction() -> void {
    if (m_control->getHalt()) {
        return;
    }
    if (auto pc = m_control->getPc()) { // starting from halt
        m_sRegs.writePc(*pc);
        m_control->clearPc();
    }

    const auto instBits = WITH_LOG_DISABLED(m_logger, m_memory->readPhysical<uint32_t>(m_sRegs.readPc() + RSP_IMEM_BASE));

    const auto inst = ISA::Instruction(instBits);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::RSP>(
            std::tuple{"PC", "0x{:04x}", static_cast<uint32_t>(m_sRegs.readPc())},
            std::tuple{"inst", "{}", inst});
    }

    const auto op   = inst.opcode;
    const auto data = inst.data;

    using namespace Opcodes;
    namespace P    = CPU::Param;
    namespace Func = CPU::Function;
    using TypeI    = ISA::CPU::TypeI;
    using TypeR    = ISA::CPU::TypeR;

    switch (op) {
        // unsupported opcodes
        case UnifiedOpcode::OP_BNEL: [[fallthrough]];
        case UnifiedOpcode::OP_BGEZL: [[fallthrough]];
        case UnifiedOpcode::OP_BEQL: [[fallthrough]];
        case UnifiedOpcode::OP_BLEZL: [[fallthrough]];
        case UnifiedOpcode::OP_SWL: [[fallthrough]];
        case UnifiedOpcode::OP_MULT: [[fallthrough]];
        case UnifiedOpcode::OP_MULTU: [[fallthrough]];
        case UnifiedOpcode::OP_DMULT: [[fallthrough]];
        case UnifiedOpcode::OP_DMULTU: [[fallthrough]];
        case UnifiedOpcode::OP_DIV: [[fallthrough]];
        case UnifiedOpcode::OP_DIVU: [[fallthrough]];
        case UnifiedOpcode::OP_DDIV: [[fallthrough]];
        case UnifiedOpcode::OP_DDIVU: [[fallthrough]];
        case UnifiedOpcode::OP_MFLO: [[fallthrough]];
        case UnifiedOpcode::OP_MFHI: [[fallthrough]];
        case UnifiedOpcode::OP_MTLO: [[fallthrough]];
        case UnifiedOpcode::OP_MTHI: [[fallthrough]];
        case UnifiedOpcode::OP_DSLL: [[fallthrough]];
        case UnifiedOpcode::OP_DSRL: [[fallthrough]];
        case UnifiedOpcode::OP_DSRA: [[fallthrough]];
        case UnifiedOpcode::OP_DSLLV: [[fallthrough]];
        case UnifiedOpcode::OP_DSRLV: [[fallthrough]];
        case UnifiedOpcode::OP_DSRAV: [[fallthrough]];
        case UnifiedOpcode::OP_DSLL32: [[fallthrough]];
        case UnifiedOpcode::OP_DSRL32: [[fallthrough]];
        case UnifiedOpcode::OP_DSRA32: [[fallthrough]];
        case UnifiedOpcode::OP_ERET:
            throw Util::Error("Unsupported RSP instruction {}", inst);

        case UnifiedOpcode::OP_SYNC: [[fallthrough]];
        case UnifiedOpcode::OP_NOP: break;

        // Jump instructions
        case UnifiedOpcode::OP_J: m_exec.executeJump<P::NO_LINK, P::IMM>(data); break;
        case UnifiedOpcode::OP_JAL: m_exec.executeJump<P::LINK, P::IMM>(data); break;
        case UnifiedOpcode::OP_JR: m_exec.executeJump<P::NO_LINK, P::REG>(data); break;
        case UnifiedOpcode::OP_JALR: m_exec.executeJump<P::LINK, P::REG>(data); break;

        // Branch instructions
        case UnifiedOpcode::OP_BNE: m_exec.executeBranch(data, Func::CMP_NE); break;
        case UnifiedOpcode::OP_BLTZ: m_exec.executeBranch(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BLTZAL: m_exec.executeBranchAndLink(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BLEZ: m_exec.executeBranch(data, Func::CMP_LEZ); break;
        case UnifiedOpcode::OP_BGEZ: m_exec.executeBranch(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BGEZAL: m_exec.executeBranchAndLink(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BEQ: m_exec.executeBranch(data, Func::CMP_EQ); break;

        // Load/Store instructions
        case UnifiedOpcode::OP_LUI: {
            auto ops = std::bit_cast<TypeI>(data);
            m_sRegs.writeGpr(ops.rt, Util::signExt32(ops.imm << 16));
            break;
        }
        case UnifiedOpcode::OP_LB: m_exec.executeMemoryOperation<P::LOAD, int8_t>(data); break;
        case UnifiedOpcode::OP_LBU: m_exec.executeMemoryOperation<P::LOAD, uint8_t>(data); break;
        case UnifiedOpcode::OP_LH: m_exec.executeMemoryOperation<P::LOAD, int16_t>(data); break;
        case UnifiedOpcode::OP_LHU: m_exec.executeMemoryOperation<P::LOAD, uint16_t>(data); break;
        case UnifiedOpcode::OP_LW: m_exec.executeMemoryOperation<P::LOAD, int32_t>(data); break;
        case UnifiedOpcode::OP_LWU: m_exec.executeMemoryOperation<P::LOAD, uint32_t>(data); break;
        case UnifiedOpcode::OP_LD: m_exec.executeMemoryOperation<P::LOAD, int64_t>(data); break;
        case UnifiedOpcode::OP_SB: m_exec.executeMemoryOperation<P::STORE, int8_t>(data); break;
        case UnifiedOpcode::OP_SH: m_exec.executeMemoryOperation<P::STORE, int16_t>(data); break;
        case UnifiedOpcode::OP_SW: m_exec.executeMemoryOperation<P::STORE, int32_t>(data); break;
        case UnifiedOpcode::OP_SD: m_exec.executeMemoryOperation<P::STORE, int64_t>(data); break;

        // Arithmetic instructions
        case UnifiedOpcode::OP_ADD: [[fallthrough]]; // TODO overflow exception
        case UnifiedOpcode::OP_ADDU: m_exec.executeBivariate(data, Func::ADD); break;
        case UnifiedOpcode::OP_ADDI: [[fallthrough]]; // TODO overflow exception
        case UnifiedOpcode::OP_ADDIU: m_exec.executeBivariateImmediate<P::SIGN_EXTEND>(data, Func::ADD); break;
        case UnifiedOpcode::OP_SLT: m_exec.executeBivariate(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_SLTU: m_exec.executeBivariate<uint32_t>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_SLTI: m_exec.executeBivariateImmediate<P::SIGN_EXTEND, int32_t>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_SLTIU: m_exec.executeBivariateImmediate<P::ZERO_EXTEND, uint32_t>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_SUBU: m_exec.executeBivariate(data, Func::SUB); break;
        case UnifiedOpcode::OP_AND: m_exec.executeBivariate(data, Func::AND); break;
        case UnifiedOpcode::OP_ANDI: m_exec.executeBivariateImmediate<P::ZERO_EXTEND>(data, Func::AND); break;
        case UnifiedOpcode::OP_OR: m_exec.executeBivariate(data, Func::OR); break;
        case UnifiedOpcode::OP_ORI: m_exec.executeBivariateImmediate<P::ZERO_EXTEND>(data, Func::OR); break;
        case UnifiedOpcode::OP_XOR: m_exec.executeBivariate(data, Func::XOR); break;
        case UnifiedOpcode::OP_XORI: m_exec.executeBivariateImmediate<P::ZERO_EXTEND>(data, Func::XOR); break;
        case UnifiedOpcode::OP_NOR: m_exec.executeBivariate(data, Func::NOR); break;

        // Shift instructions
        case UnifiedOpcode::OP_SLL: m_exec.executeShift<P::WORD, P::LEFT, P::LOGICAL>(data); break;
        case UnifiedOpcode::OP_SRL: m_exec.executeShift<P::WORD, P::RIGHT, P::LOGICAL>(data); break;
        case UnifiedOpcode::OP_SRA: m_exec.executeShift<P::WORD, P::RIGHT, P::ARITHMETIC>(data); break;
        case UnifiedOpcode::OP_SLLV: m_exec.executeShift<P::WORD, P::LEFT, P::LOGICAL, P::VARIABLE>(data); break;
        case UnifiedOpcode::OP_SRLV: m_exec.executeShift<P::WORD, P::RIGHT, P::LOGICAL, P::VARIABLE>(data); break;
        case UnifiedOpcode::OP_SRAV: m_exec.executeShift<P::WORD, P::RIGHT, P::ARITHMETIC, P::VARIABLE>(data); break;

        // Coprocessor instructions
        case UnifiedOpcode::OP_MFCz: {
            auto cp = (data >> 26) & 0b11;
            if (cp != 0) throw Util::Error("Unsupported instruction on coprocessor {}", cp);
            auto ops = std::bit_cast<TypeR>(data);
            m_sRegs.writeGpr(ops.rt, m_control->readRegister(ops.rd));

            break;
        }
        case UnifiedOpcode::OP_MTCz: {
            auto cp = (data >> 26) & 0b11;
            if (cp != 0) throw Util::Error("Unsupported instruction on coprocessor {}", cp);
            auto ops = std::bit_cast<TypeR>(data);
            m_control->writeRegister(ops.rd, m_sRegs.readGpr(ops.rt));
            break;
        }

        // Misc. instructions
        case UnifiedOpcode::OP_TLBR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWI: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBP: [[fallthrough]];
        case UnifiedOpcode::OP_CACHE:
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RSP>("ignored instruction {}", inst);
            }
            break;
        default:
            throw Util::Error("Unimplemented instruction @ PC {:#08x}: {} ({:#08x})", m_sRegs.readPc(), inst, data);
    }

    m_sRegs.advancePc();

    if (m_control->getSingleStep()) {
        halt();
    }
}

auto RSP::halt() -> void {
    // save PC when halting
    m_control->setPc(m_sRegs.readPc());
    m_control->setHalt(true);
}

} // namespace RSP
