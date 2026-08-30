module;

#include <util/defines.hpp>

export module CPU:CPU;

import std;
import CP0;
import CP1;
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
        CP0::CP0*                     cp0,
        CP1::CP1*                     cp1)
        : m_regs(logger), m_logger(logger), m_memory(memory), m_cp0(cp0), m_cp1(cp1), m_exec(logger, &m_regs, m_memory) {
        m_regs.writePc(0xBFC00000);
    }

    auto registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void;

    auto checkInterrupts() -> void;
    auto runInstruction() -> void;

    auto emulateInitialBoot() -> void;

    auto dumpIMem(std::filesystem::path file) -> void;

  private:
    Registers<Sys::CPU>                  m_regs;
    std::shared_ptr<Util::Logger>        m_logger;
    Memory::Memory*                      m_memory;
    CP0::CP0*                            m_cp0;
    CP1::CP1*                            m_cp1;
    ::CPU::InstructionExecutor<Sys::CPU> m_exec;

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

    m_cp0->writeReg<ISA::CP0_REG::RANDOM>(static_cast<uint32_t>(0x0000001F));
    m_cp0->writeReg<ISA::CP0_REG::STATUS>(static_cast<uint32_t>(0x34000000));
    m_cp0->writeReg<ISA::CP0_REG::PRID>(static_cast<uint32_t>(0x00000B00));
    m_cp0->writeReg<ISA::CP0_REG::CONFIG>(static_cast<uint32_t>(0x0006E463));
}

auto CPU::registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void {
    m_bootAddress  = callbackBootAddress;
    m_bootCallback = std::move(callback);
}

auto CPU::checkInterrupts() -> void {
    if (m_cp0->hasInterrupt()) {
        auto status = m_cp0->readReg<ISA::CP0_REG::STATUS>();
        status.exl  = 1;
        m_cp0->writeReg(status);
        auto nextPc = m_regs.pcIsDelaySlot() ? m_regs.readPc() - 4 : m_regs.readPc();
        m_cp0->writeReg<ISA::CP0_REG::EPC>(nextPc);
        m_regs.writePc(status.bev ? 0xBFC00000 : 0x80000000);
        m_regs.clearDelaySlot();
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::HIGH, Sys::CPU>(
                std::tuple{"interruptStatus", "{:#08x}", std::bit_cast<uint32_t>(status)},
                std::tuple{"interruptCause", "{:#08x}", std::bit_cast<uint32_t>(m_cp0->readReg<ISA::CP0_REG::CAUSE>())},
                std::tuple{"returnPc", "{:#08x}", nextPc});
        }
    }
}

auto CPU::runInstruction() -> void {
    using namespace Opcodes;
    namespace P        = Param;
    namespace Func     = Util::Function;
    namespace FpExcept = CP1::ExceptionFunc;
    using TypeI        = ISA::CPU::TypeI;
    using TypeR        = ISA::CPU::TypeR;

    // boot callback
    if (!m_hasBooted && static_cast<uint32_t>(m_regs.readPc()) == m_bootAddress) {
        m_hasBooted = true;
        m_bootCallback();
    }

    const auto instBits = WITH_LOG_DISABLED(m_logger, m_memory->read<uint32_t>(m_regs.readPc()));
    const auto inst     = ISA::Instruction(instBits);

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
        case UnifiedOpcode::OP_BEQ: m_exec.executeBranch(data, Func::CMP_EQ); break;
        case UnifiedOpcode::OP_BNE: m_exec.executeBranch(data, Func::CMP_NE); break;
        case UnifiedOpcode::OP_BLEZ: m_exec.executeBranch(data, Func::CMP_LEZ); break;
        case UnifiedOpcode::OP_BGTZ: m_exec.executeBranch(data, Func::CMP_GTZ); break;
        case UnifiedOpcode::OP_BLTZ: m_exec.executeBranch(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BGEZ: m_exec.executeBranch(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BLTZAL: m_exec.executeBranchAndLink(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BGEZAL: m_exec.executeBranchAndLink(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BEQL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_EQ); break;
        case UnifiedOpcode::OP_BNEL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_NE); break;
        case UnifiedOpcode::OP_BLEZL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_LEZ); break;
        case UnifiedOpcode::OP_BGTZL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_GTZ); break;
        case UnifiedOpcode::OP_BLTZL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BGEZL: m_exec.executeBranch<P::LIKELY>(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BLTZALL: m_exec.executeBranchAndLink<P::LIKELY>(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BGEZALL: m_exec.executeBranchAndLink<P::LIKELY>(data, Func::CMP_GEZ); break;

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
        case UnifiedOpcode::OP_SWL: m_exec.executeMemoryOperationUnaligned<P::STORE, P::LEFT, int32_t>(data); break;
        case UnifiedOpcode::OP_SWR: m_exec.executeMemoryOperationUnaligned<P::STORE, P::RIGHT, int32_t>(data); break;
        case UnifiedOpcode::OP_LWL: m_exec.executeMemoryOperationUnaligned<P::LOAD, P::LEFT, int32_t>(data); break;
        case UnifiedOpcode::OP_LWR: m_exec.executeMemoryOperationUnaligned<P::LOAD, P::RIGHT, int32_t>(data); break;

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
        case UnifiedOpcode::OP_SUB: [[fallthrough]]; // TODO overflow exception
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

        // FPU instructions
        case UnifiedOpcode::OP_CVT_S_FMT: m_cp1->getExec()->executeConvert<float>(data); break;
        case UnifiedOpcode::OP_CVT_D_FMT: m_cp1->getExec()->executeConvert<double>(data); break;
        case UnifiedOpcode::OP_CVT_W_FMT: m_cp1->getExec()->executeConvert<uint32_t>(data); break;
        case UnifiedOpcode::OP_CVT_L_FMT: m_cp1->getExec()->executeConvert<uint64_t>(data); break;

        case UnifiedOpcode::OP_ADD_FMT: m_cp1->getExec()->executeBivariate(data, Func::ADD, FpExcept::ADD); break;
        case UnifiedOpcode::OP_SUB_FMT: m_cp1->getExec()->executeBivariate(data, Func::SUB, FpExcept::ADD); break;
        case UnifiedOpcode::OP_MUL_FMT: m_cp1->getExec()->executeBivariate(data, Func::MUL, FpExcept::MUL); break;
        case UnifiedOpcode::OP_DIV_FMT: m_cp1->getExec()->executeBivariate(data, Func::DIV, FpExcept::DIV); break;
        case UnifiedOpcode::OP_SQRT_FMT: m_cp1->getExec()->executeBivariate(data, Func::SQRT, FpExcept::SQRT); break;
        case UnifiedOpcode::OP_ABS_FMT: m_cp1->getExec()->executeBivariate(data, Func::ABS, FpExcept::ABS); break;
        case UnifiedOpcode::OP_NEG_FMT: m_cp1->getExec()->executeBivariate(data, Func::NEG, FpExcept::NEG); break;
        case UnifiedOpcode::OP_MOV_FMT: m_cp1->getExec()->executeBivariate(data, Func::NOP); break;

        case UnifiedOpcode::OP_ROUND_W_FMT: m_cp1->getExec()->executeConvert<uint32_t>(data, Util::FP_ROUND_MODE::NEAREST); break;
        case UnifiedOpcode::OP_ROUND_L_FMT: m_cp1->getExec()->executeConvert<uint64_t>(data, Util::FP_ROUND_MODE::NEAREST); break;
        case UnifiedOpcode::OP_TRUNC_W_FMT: m_cp1->getExec()->executeConvert<uint32_t>(data, Util::FP_ROUND_MODE::TO_ZERO); break;
        case UnifiedOpcode::OP_TRUNC_L_FMT: m_cp1->getExec()->executeConvert<uint64_t>(data, Util::FP_ROUND_MODE::TO_ZERO); break;
        case UnifiedOpcode::OP_CEIL_W_FMT: m_cp1->getExec()->executeConvert<uint32_t>(data, Util::FP_ROUND_MODE::UP); break;
        case UnifiedOpcode::OP_CEIL_L_FMT: m_cp1->getExec()->executeConvert<uint64_t>(data, Util::FP_ROUND_MODE::UP); break;
        case UnifiedOpcode::OP_FLOOR_W_FMT: m_cp1->getExec()->executeConvert<uint32_t>(data, Util::FP_ROUND_MODE::DOWN); break;
        case UnifiedOpcode::OP_FLOOR_L_FMT: m_cp1->getExec()->executeConvert<uint64_t>(data, Util::FP_ROUND_MODE::DOWN); break;

        case UnifiedOpcode::OP_C_F_FMT: m_cp1->getExec()->executeCompare<Param::ORDERED>(data, Func::FALSE); break;
        case UnifiedOpcode::OP_C_UN_FMT: m_cp1->getExec()->executeCompare<Param::UNORDERED>(data, Func::FALSE); break;
        case UnifiedOpcode::OP_C_EQ_FMT: m_cp1->getExec()->executeCompare<Param::ORDERED>(data, Func::CMP_EQ); break;
        case UnifiedOpcode::OP_C_UEQ_FMT: m_cp1->getExec()->executeCompare<Param::UNORDERED>(data, Func::CMP_EQ); break;
        case UnifiedOpcode::OP_C_OLT_FMT: m_cp1->getExec()->executeCompare<Param::ORDERED>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_C_ULT_FMT: m_cp1->getExec()->executeCompare<Param::UNORDERED>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_C_OLE_FMT: m_cp1->getExec()->executeCompare<Param::ORDERED>(data, Func::CMP_LE); break;
        case UnifiedOpcode::OP_C_ULE_FMT: m_cp1->getExec()->executeCompare<Param::UNORDERED>(data, Func::CMP_LE); break;
        case UnifiedOpcode::OP_C_SF_FMT: m_cp1->getExec()->executeCompare<Param::ORDERED, Param::SIGNAL>(data, Func::FALSE); break;
        case UnifiedOpcode::OP_C_NGLE_FMT: m_cp1->getExec()->executeCompare<Param::UNORDERED, Param::SIGNAL>(data, Func::FALSE); break;
        case UnifiedOpcode::OP_C_SEQ_FMT: m_cp1->getExec()->executeCompare<Param::ORDERED, Param::SIGNAL>(data, Func::CMP_EQ); break;
        case UnifiedOpcode::OP_C_NGL_FMT: m_cp1->getExec()->executeCompare<Param::UNORDERED, Param::SIGNAL>(data, Func::CMP_EQ); break;
        case UnifiedOpcode::OP_C_LT_FMT: m_cp1->getExec()->executeCompare<Param::ORDERED, Param::SIGNAL>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_C_NGE_FMT: m_cp1->getExec()->executeCompare<Param::UNORDERED, Param::SIGNAL>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_C_LE_FMT: m_cp1->getExec()->executeCompare<Param::ORDERED, Param::SIGNAL>(data, Func::CMP_LE); break;
        case UnifiedOpcode::OP_C_NGT_FMT: m_cp1->getExec()->executeCompare<Param::UNORDERED, Param::SIGNAL>(data, Func::CMP_LE); break;

        // Coprocessor instructions
        case UnifiedOpcode::OP_BCzT:
            if (inst.getCoprocessor() == 1) {
                m_exec.executeBranch(data, m_cp1->getRegs()->readStatus().c);
            } else {
                throw Util::Error("Unsupported instruction on coprocessor {}: {}", inst.getCoprocessor(), inst);
            }
            break;
        case UnifiedOpcode::OP_BCzF:
            if (inst.getCoprocessor() == 1) {
                m_exec.executeBranch(data, !m_cp1->getRegs()->readStatus().c);
            } else {
                throw Util::Error("Unsupported instruction on coprocessor {}: {}", inst.getCoprocessor(), inst);
            }
            break;
        case UnifiedOpcode::OP_BCzTL:
            if (inst.getCoprocessor() == 1) {
                m_exec.executeBranch<Param::LIKELY>(data, m_cp1->getRegs()->readStatus().c);
            } else {
                throw Util::Error("Unsupported instruction on coprocessor {}: {}", inst.getCoprocessor(), inst);
            }
            break;
        case UnifiedOpcode::OP_BCzFL:
            if (inst.getCoprocessor() == 1) {
                m_exec.executeBranch<Param::LIKELY>(data, !m_cp1->getRegs()->readStatus().c);
            } else {
                throw Util::Error("Unsupported instruction on coprocessor {}: {}", inst.getCoprocessor(), inst);
            }
            break;

        case UnifiedOpcode::OP_LWC1: {
            const auto ops = std::bit_cast<ISA::CPU::TypeI>(data);
            const auto rs  = m_regs.readGpr(ops.rs);
            m_cp1->getExec()->executeMemoryOperation<Param::LOAD_F, uint32_t>(data, rs);
            break;
        }
        case UnifiedOpcode::OP_SWC1: {
            const auto ops = std::bit_cast<ISA::CPU::TypeI>(data);
            const auto rs  = m_regs.readGpr(ops.rs);
            m_cp1->getExec()->executeMemoryOperation<Param::STORE_F, uint32_t>(data, rs);
            break;
        }
        case UnifiedOpcode::OP_LDC1: {
            const auto ops = std::bit_cast<ISA::CPU::TypeI>(data);
            const auto rs  = m_regs.readGpr(ops.rs);
            m_cp1->getExec()->executeMemoryOperation<Param::LOAD_F, uint64_t>(data, rs);
            break;
        }
        case UnifiedOpcode::OP_SDC1: {
            const auto ops = std::bit_cast<ISA::CPU::TypeI>(data);
            const auto rs  = m_regs.readGpr(ops.rs);
            m_cp1->getExec()->executeMemoryOperation<Param::STORE_F, uint64_t>(data, rs);
            break;
        }
        case UnifiedOpcode::OP_MFCz: {
            const auto cp  = inst.getCoprocessor();
            const auto ops = std::bit_cast<TypeR>(data);
            switch (cp) {
                case 0: m_regs.writeGpr(ops.rt, m_cp0->readReg(ops.rd)); break;
                case 1: m_regs.writeGpr(ops.rt, m_cp1->getRegs()->readFgr<uint32_t>(ops.rd)); break;
                default: throw Util::Error("Unsupported instruction on coprocessor {}: {}", cp, inst);
            }
            break;
        }
        case UnifiedOpcode::OP_MTCz: {
            const auto cp  = inst.getCoprocessor();
            const auto ops = std::bit_cast<TypeR>(data);
            switch (cp) {
                case 0: m_cp0->writeReg(ops.rd, m_regs.readGpr(ops.rt)); break;
                case 1: m_cp1->getRegs()->writeFgr<uint32_t>(ops.rd, m_regs.readGpr<uint32_t>(ops.rt)); break;
                default: throw Util::Error("Unsupported instruction on coprocessor {}: {}", cp, inst);
            }
            break;
        }
        case UnifiedOpcode::OP_CFCz: {
            const auto cp  = inst.getCoprocessor();
            const auto ops = std::bit_cast<TypeR>(data);
            switch (cp) {
                case 1:
                    switch (ops.rd) {
                        case 0: m_regs.writeGpr(ops.rt, std::bit_cast<uint32_t>(m_cp1->getRegs()->readRevision())); break;
                        case 31: m_regs.writeGpr(ops.rt, std::bit_cast<uint32_t>(m_cp1->getRegs()->readStatus())); break;
                        default: throw Util::Error("Unsupported CP1 control register {}", ops.rd);
                    }
                    break;
                default: throw Util::Error("Unsupported instruction on coprocessor {}: {}", cp, inst);
            }
            break;
        }
        case UnifiedOpcode::OP_CTCz: {
            const auto cp  = inst.getCoprocessor();
            const auto ops = std::bit_cast<TypeR>(data);
            switch (cp) {
                case 1:
                    switch (ops.rd) {
                        case 31: m_cp1->getRegs()->writeStatus(m_regs.readGpr(ops.rt)); break;
                        default: throw Util::Error("Unsupported CP1 control register {}", ops.rd);
                    }
                    break;
                default: throw Util::Error("Unsupported instruction on coprocessor {}: {}", cp, inst);
            }
            break;
        }

        // Misc. instructions
        case UnifiedOpcode::OP_TLBR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWI: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBP: [[fallthrough]];
        case UnifiedOpcode::OP_CACHE:
            // IF_LOG_ENABLED(m_logger) {
            //     m_logger->log<Level::HIGH, Sev::WARNING, Sys::CPU>("Ignored instruction {}", inst);
            // }
            break;
        case UnifiedOpcode::OP_ERET: {
            auto status = m_cp0->readReg<ISA::CP0_REG::STATUS>();
            if (status.erl) {
                m_regs.writePc(m_cp0->readReg<ISA::CP0_REG::ERROREPC>());
                status.erl = 0;
            } else {
                m_regs.writePc(m_cp0->readReg<ISA::CP0_REG::EPC>());
                status.exl = 0;
            }
            m_cp0->writeReg(status);
            break;
        }
        default:
            dumpIMem("cpu_imem.txt");
            throw Util::Error("CPU unimplemented instruction @ PC {:#08x}: {} ({:#08x})", m_regs.readPc(), inst, data);
    }

    if (op != UnifiedOpcode::OP_ERET) {
        m_regs.advancePc();
    }
}

auto CPU::dumpIMem(std::filesystem::path file) -> void {
    auto romDumper = Util::Logger(file);
    romDumper.setLevel(Level::MAX);
    for (auto addr = 0u; addr < Memory::rangeOf(Memory::PhysSeg::RDRAM).upper; addr += 4) {
        const auto word = m_memory->readPhysical<uint32_t>(addr);
        romDumper.print("{:#010x}: {}", Memory::rangeOf(Memory::VirtSeg::KSEG0).lower + addr, ISA::Instruction(word));
    }
    romDumper.flush();
}

} // namespace CPU
