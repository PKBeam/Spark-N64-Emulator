module;

#include <util/defines.hpp>

export module CPU:CPU;

import std;
import CP0;
import ISA;
import Memory;
import Util;

import :InstructionExecutor;
import :Registers;

export namespace CPU {

class CPU {
  public:
    CPU(std::shared_ptr<Util::Logger> logger,
        Memory::Memory*               memory,
        CP0::CP0*                     cp0)
        : m_regs(logger), m_logger(logger), m_memory(memory), m_cp0(cp0), m_exec(logger, &m_regs, m_memory) {
        m_regs.writePc(0xBFC00000);
    }

    auto registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void;

    auto checkInterrupts() -> void;
    auto runInstruction() -> void;

    auto emulateInitialBoot() -> void;

  private:
    Registers<Sys::CPU>                                m_regs;
    std::shared_ptr<Util::Logger>                      m_logger;
    Memory::Memory*                                    m_memory;
    CP0::CP0*                                          m_cp0;
    InstructionExecutor::InstructionExecutor<Sys::CPU> m_exec;

    bool                  m_hasBooted{};
    std::function<void()> m_bootCallback;
    uint32_t              m_bootAddress{};
};

auto CPU::emulateInitialBoot() -> void {
    // DMA 1 MiB of ROM code into RSP DMEM
    // these need to be done in 32-bit chunks to ensure correct endianness
    for (auto i = 0uz; i < 0x1000; i += 4) {
        const auto word = m_memory->read<uint32_t>(0xB0000000 + i);
        m_memory->write<uint32_t>(0xA4000000 + i, word);
    }
    m_regs.writePc(static_cast<uint32_t>(0xA4000040));

    m_regs.writeGpr<ISA::CPU_REG::t3>(static_cast<uint32_t>(0xA4000040));
    m_regs.writeGpr<ISA::CPU_REG::s4>(static_cast<uint32_t>(0x00000001));

    // TODO detect CIC and set this accordingly so we don't fail the checksum
    // CIC_6102 : 0x3F
    // CIC_6105 : 0x91
    m_regs.writeGpr<ISA::CPU_REG::s6>(static_cast<uint32_t>(0x00000091));
    m_regs.writeGpr<ISA::CPU_REG::sp>(static_cast<uint32_t>(0xA4001FF0));
    m_regs.writeGpr<ISA::CPU_REG::ra>(static_cast<uint32_t>(0xA4001000)); // from IPL2 stage

    m_cp0->writeReg<CP0::Registers::RANDOM>(static_cast<uint32_t>(0x0000001F));
    m_cp0->writeReg<CP0::Registers::STATUS>(static_cast<uint32_t>(0x34000000));
    m_cp0->writeReg<CP0::Registers::PRID>(static_cast<uint32_t>(0x00000B00));
    m_cp0->writeReg<CP0::Registers::CONFIG>(static_cast<uint32_t>(0x0006E463));
}

auto CPU::registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void {
    m_bootAddress  = callbackBootAddress;
    m_bootCallback = std::move(callback);
}

auto CPU::checkInterrupts() -> void {
    if (m_cp0->hasInterrupt()) {
        auto status = m_cp0->readReg<CP0::Registers::STATUS>();
        status.exl  = 1;
        m_cp0->writeReg(status);
        auto nextPc = m_regs.pcIsDelaySlot() ? m_regs.readPc() - 4 : m_regs.readPc();
        m_cp0->writeReg<CP0::Registers::EPC>(nextPc);
        m_regs.writePc(status.bev ? 0xBFC00000 : 0x80000000);
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::HIGH, Sys::CPU>(
                std::tuple{"exceptionStatus", "{:#08x}", std::bit_cast<uint32_t>(status)},
                std::tuple{"exceptionCause", "{:#08x}", std::bit_cast<uint32_t>(m_cp0->readReg<CP0::Registers::CAUSE>())},
                std::tuple{"returnPc", "{:#08x}", nextPc});
        }
    }
}

auto CPU::runInstruction() -> void {
    namespace P    = InstructionExecutor::Param;
    namespace Func = InstructionExecutor::Function;
    using namespace Opcodes;
    using TypeI = ISA::CPU::TypeI;
    using TypeR = ISA::CPU::TypeR;

    const auto instBits = WITH_LOG_DISABLED(m_logger, m_memory->read<uint32_t>(m_regs.readPc()));

    const auto inst = ISA::Instruction(instBits);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::CPU>(
            std::tuple{"PC", "0x{:08x}", static_cast<uint32_t>(m_regs.readPc())},
            std::tuple{"inst", "{}", inst});
    }

    const auto op   = inst.opcode;
    const auto data = inst.data;

    switch (op) {
        case UnifiedOpcode::OP_SYNC: [[fallthrough]];
        case UnifiedOpcode::OP_NOP: break;

        // Jump instructions
        case UnifiedOpcode::OP_J: m_exec.executeJump<P::NO_LINK, P::IMM>(data); break;
        case UnifiedOpcode::OP_JAL: m_exec.executeJump<P::LINK, P::IMM>(data); break;
        case UnifiedOpcode::OP_JR: m_exec.executeJump<P::NO_LINK, P::REG>(data); break;
        case UnifiedOpcode::OP_JALR: m_exec.executeJump<P::LINK, P::REG>(data); break;

        // Branch instructions
        case UnifiedOpcode::OP_BNE: m_exec.executeBranch(data, Func::CMP_NE); break;
        case UnifiedOpcode::OP_BNEL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_NE); break;
        case UnifiedOpcode::OP_BLTZ: m_exec.executeBranch(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BLTZAL: m_exec.executeBranchAndLink(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BLEZ: m_exec.executeBranch(data, Func::CMP_LEZ); break;
        case UnifiedOpcode::OP_BLEZL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_LEZ); break;
        case UnifiedOpcode::OP_BGEZ: m_exec.executeBranch(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BGEZL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BGEZAL: m_exec.executeBranchAndLink(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BEQ: m_exec.executeBranch(data, Func::CMP_EQ); break;
        case UnifiedOpcode::OP_BEQL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_EQ); break;

        // Load/Store instructions
        case UnifiedOpcode::OP_LUI: {
            auto ops = std::bit_cast<TypeI>(data);
            m_regs.writeGpr(ops.rt, Util::signExt32(ops.imm << 16));
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
        case UnifiedOpcode::OP_SWL: {
            auto ops   = std::bit_cast<TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + m_regs.readGpr(ops.rs);
            for (auto byte = 4z; byte > vaddr % 4; --byte) {
                auto thisByte = (m_regs.readGpr<uint32_t>(ops.rt) >> (8 * byte)) & 0xFF;
                m_memory->write<uint8_t>(vaddr + (4 - byte), thisByte);
            }
            break;
        }

        // Arithmetic instructions
        case UnifiedOpcode::OP_MULT: m_exec.executeMultiply<int32_t>(data); break;
        case UnifiedOpcode::OP_MULTU: m_exec.executeMultiply<uint32_t>(data); break;
        case UnifiedOpcode::OP_DMULT: m_exec.executeMultiply<int64_t>(data); break;
        case UnifiedOpcode::OP_DMULTU: m_exec.executeMultiply<uint64_t>(data); break;
        case UnifiedOpcode::OP_DIV: m_exec.executeDivide<int32_t>(data); break;
        case UnifiedOpcode::OP_DIVU: m_exec.executeDivide<uint32_t>(data); break;
        case UnifiedOpcode::OP_DDIV: m_exec.executeDivide<int64_t>(data); break;
        case UnifiedOpcode::OP_DDIVU: m_exec.executeDivide<uint64_t>(data); break;
        case UnifiedOpcode::OP_MFLO: m_regs.writeGpr(std::bit_cast<TypeR>(data).rd, m_regs.readLo()); break;
        case UnifiedOpcode::OP_MFHI: m_regs.writeGpr(std::bit_cast<TypeR>(data).rd, m_regs.readHi()); break;
        case UnifiedOpcode::OP_MTLO: m_regs.writeLo(m_regs.readGpr(std::bit_cast<TypeR>(data).rs)); break;
        case UnifiedOpcode::OP_MTHI: m_regs.writeHi(m_regs.readGpr(std::bit_cast<TypeR>(data).rs)); break;
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
        // clang-format off
        case UnifiedOpcode::OP_SLL:    m_exec.executeShift<P::WORD,   P::LEFT,  P::LOGICAL                         >(data); break;
        case UnifiedOpcode::OP_SRL:    m_exec.executeShift<P::WORD,   P::RIGHT, P::LOGICAL                         >(data); break;
        case UnifiedOpcode::OP_SRA:    m_exec.executeShift<P::WORD,   P::RIGHT, P::ARITHMETIC                      >(data); break;
        case UnifiedOpcode::OP_SLLV:   m_exec.executeShift<P::WORD,   P::LEFT,  P::LOGICAL,    P::VARIABLE         >(data); break;
        case UnifiedOpcode::OP_SRLV:   m_exec.executeShift<P::WORD,   P::RIGHT, P::LOGICAL,    P::VARIABLE         >(data); break;
        case UnifiedOpcode::OP_SRAV:   m_exec.executeShift<P::WORD,   P::RIGHT, P::ARITHMETIC, P::VARIABLE         >(data); break;
        case UnifiedOpcode::OP_DSLL:   m_exec.executeShift<P::DOUBLE, P::LEFT,  P::LOGICAL                         >(data); break;
        case UnifiedOpcode::OP_DSRL:   m_exec.executeShift<P::DOUBLE, P::RIGHT, P::LOGICAL                         >(data); break;
        case UnifiedOpcode::OP_DSRA:   m_exec.executeShift<P::DOUBLE, P::RIGHT, P::ARITHMETIC                      >(data); break;
        case UnifiedOpcode::OP_DSLLV:  m_exec.executeShift<P::DOUBLE, P::LEFT,  P::LOGICAL,    P::VARIABLE         >(data); break;
        case UnifiedOpcode::OP_DSRLV:  m_exec.executeShift<P::DOUBLE, P::RIGHT, P::LOGICAL,    P::VARIABLE         >(data); break;
        case UnifiedOpcode::OP_DSRAV:  m_exec.executeShift<P::DOUBLE, P::RIGHT, P::ARITHMETIC, P::VARIABLE         >(data); break;
        case UnifiedOpcode::OP_DSLL32: m_exec.executeShift<P::DOUBLE, P::LEFT,  P::LOGICAL,    P::FIXED,   P::ADD32>(data); break;
        case UnifiedOpcode::OP_DSRL32: m_exec.executeShift<P::DOUBLE, P::RIGHT, P::LOGICAL,    P::FIXED,   P::ADD32>(data); break;
        case UnifiedOpcode::OP_DSRA32: m_exec.executeShift<P::DOUBLE, P::RIGHT, P::ARITHMETIC, P::FIXED,   P::ADD32>(data); break;
            // clang-format on

        // Coprocessor instructions
        case UnifiedOpcode::OP_MFCz: {
            auto cp = (data >> 26) & 0b11;
            if (cp != 0) throw Util::Error("Unsupported instruction on coprocessor {}", cp);
            auto ops = std::bit_cast<TypeR>(data);
            m_regs.writeGpr(ops.rt, m_cp0->readReg(ops.rd));
            break;
        }
        case UnifiedOpcode::OP_MTCz: {
            auto cp = (data >> 26) & 0b11;
            if (cp != 0) throw Util::Error("Unsupported instruction on coprocessor {}", cp);
            auto ops = std::bit_cast<TypeR>(data);
            m_cp0->writeReg(ops.rd, m_regs.readGpr(ops.rt));
            break;
        }
        case UnifiedOpcode::OP_CFCz: {
            auto cp  = (data >> 26) & 0b11;
            auto ops = std::bit_cast<TypeR>(data);
            if (cp == 1 && ops.rd == 31) {
                m_regs.writeGpr(ops.rt, 0);
                break;
            }
            throw Util::Error("Unsupported instruction on coprocessor {}", cp);
        }
        case UnifiedOpcode::OP_CTCz: {
            auto cp  = (data >> 26) & 0b11;
            auto ops = std::bit_cast<TypeR>(data);
            if (cp == 1 && ops.rd == 31) {
                m_regs.writeGpr(ops.rt, 0);
                break;
            }
            throw Util::Error("Unsupported instruction on coprocessor {}", cp);
        }

        // Misc. instructions
        case UnifiedOpcode::OP_TLBR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWI: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBP: [[fallthrough]];
        case UnifiedOpcode::OP_CACHE:
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::CPU>("Ignored instruction {}", inst);
            }
            break;
        case UnifiedOpcode::OP_ERET: {
            auto status = m_cp0->readReg<CP0::Registers::STATUS>();
            if (status.erl) {
                m_regs.writePc(m_cp0->readReg<CP0::Registers::ERROREPC>());
                status.erl = 0;
            } else {
                m_regs.writePc(m_cp0->readReg<CP0::Registers::EPC>());
                status.exl = 0;
            }
            m_cp0->writeReg(status);
            break;
        }
        default:
            throw Util::Error("Unimplemented instruction @ PC {:#08x}: {} ({:#08x})", m_regs.readPc(), inst, data);
    }

    // boot callback
    if (op == UnifiedOpcode::OP_JR && m_regs.getNextPc() == m_bootAddress) {
        m_hasBooted = true;
        m_bootCallback();
    }

    // hang detection
    if (op == UnifiedOpcode::OP_BGEZAL) {
        const auto ops = std::bit_cast<TypeI>(data);
        if (static_cast<int16_t>(ops.imm) == -1 &&                 // branches to previous instruction
            !m_hasBooted &&                                        // in early boot
            ops.rs == static_cast<uint32_t>(ISA::CPU_REG::zero) && // is unconditional branch
            m_memory->read<uint32_t>(m_regs.readPc() - 4) == 0)    // branches to NOP
        {
            throw Util::Error("Detected infinite looping BGEZAL @ PC {:#08x}, likely boot checksum fail.", m_regs.readPc());
        }
    }

    if (op != UnifiedOpcode::OP_ERET) {
        m_regs.advancePc();
    }
}

} // namespace CPU
