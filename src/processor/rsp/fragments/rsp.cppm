module;

#include <util/defines.hpp>

export module RSP:RSP;

import std;
import CP0;
import CPU;
import ISA;
import Interfaces;
import InterfaceTypes;
import Memory;
import RspControl;
import Util;

import :InstructionExecutor;
import :Registers;

using Control = RSP::Control;

export namespace RSP {

class RSP {
  public:
    RSP(std::shared_ptr<Util::Logger> logger, Memory::Memory* memory, Control* control, Interfaces::MipsInterface* mipsInterface)
        : m_logger(logger), m_memory(memory), m_gprs(logger), m_vprs(logger), m_control(control), m_exec(m_logger, &m_gprs, &m_vprs, m_memory), m_mipsInterface(mipsInterface) {};

    auto runInstruction() -> void;

    auto halt() -> void;

    auto dumpIMem(std::filesystem::path file) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    Memory::Memory*               m_memory{};
    CPU::Registers<Sys::RSP>      m_gprs;
    ::RSP::Registers              m_vprs;
    Control*                      m_control{};
    ::RSP::InstructionExecutor    m_exec;
    std::optional<VirtualAddr>    m_delaySlotPc;
    Interfaces::MipsInterface*    m_mipsInterface{};
};

auto RSP::runInstruction() -> void {
    if (m_control->getHalt()) {
        return;
    }

    if (auto pc = m_control->getPc()) { // starting from halt
        m_gprs.writePc(*pc);
        m_control->clearPc();

        // static int imems = 0;
        // dumpIMem(std::format("rsp_imem_{}.txt", imems++));
        // std::println("RSP IMEM dumped to rsp_imem_{}.txt", imems - 1);
    }

    const auto instBits = WITH_LOG_DISABLED(m_logger, m_memory->readPhysical<uint32_t>(m_gprs.readPc() + RSP_IMEM_BASE));
    const auto inst     = ISA::Instruction(instBits);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::RSP>(
            std::tuple{"PC", "0x{:04x}", static_cast<uint32_t>(m_gprs.readPc())},
            std::tuple{"inst", "{}", inst});
    }

    const auto op   = inst.opcode;
    const auto data = inst.data;

    using namespace Opcodes;
    namespace P    = Param;
    namespace Func = Util::Function;
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
        case UnifiedOpcode::OP_J: m_exec.cpuExec()->executeJump<P::NO_LINK, P::IMM>(data); break;
        case UnifiedOpcode::OP_JAL: m_exec.cpuExec()->executeJump<P::LINK, P::IMM>(data); break;
        case UnifiedOpcode::OP_JR: m_exec.cpuExec()->executeJump<P::NO_LINK, P::REG>(data); break;
        case UnifiedOpcode::OP_JALR: m_exec.cpuExec()->executeJump<P::LINK, P::REG>(data); break;

        // Branch instructions
        case UnifiedOpcode::OP_BEQ: m_exec.cpuExec()->executeBranch(data, Func::CMP_EQ); break;
        case UnifiedOpcode::OP_BNE: m_exec.cpuExec()->executeBranch(data, Func::CMP_NE); break;
        case UnifiedOpcode::OP_BLEZ: m_exec.cpuExec()->executeBranch(data, Func::CMP_LEZ); break;
        case UnifiedOpcode::OP_BGTZ: m_exec.cpuExec()->executeBranch(data, Func::CMP_GTZ); break;
        case UnifiedOpcode::OP_BLTZ: m_exec.cpuExec()->executeBranch(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BGEZ: m_exec.cpuExec()->executeBranch(data, Func::CMP_GEZ); break;
        case UnifiedOpcode::OP_BLTZAL: m_exec.cpuExec()->executeBranchAndLink(data, Func::CMP_LTZ); break;
        case UnifiedOpcode::OP_BGEZAL: m_exec.cpuExec()->executeBranchAndLink(data, Func::CMP_GEZ); break;
        // Load/Store instructions
        case UnifiedOpcode::OP_LUI: {
            auto ops = std::bit_cast<TypeI>(data);
            m_gprs.writeGpr(ops.rt, Util::signExt32(ops.imm << 16));
            break;
        }
        case UnifiedOpcode::OP_LB: m_exec.cpuExec()->executeMemoryOperation<P::LOAD, int8_t>(data); break;
        case UnifiedOpcode::OP_LBU: m_exec.cpuExec()->executeMemoryOperation<P::LOAD, uint8_t>(data); break;
        case UnifiedOpcode::OP_LH: m_exec.cpuExec()->executeMemoryOperation<P::LOAD, int16_t>(data); break;
        case UnifiedOpcode::OP_LHU: m_exec.cpuExec()->executeMemoryOperation<P::LOAD, uint16_t>(data); break;
        case UnifiedOpcode::OP_LW: m_exec.cpuExec()->executeMemoryOperation<P::LOAD, int32_t>(data); break;
        case UnifiedOpcode::OP_LWU: m_exec.cpuExec()->executeMemoryOperation<P::LOAD, uint32_t>(data); break;
        case UnifiedOpcode::OP_LD: m_exec.cpuExec()->executeMemoryOperation<P::LOAD, int64_t>(data); break;
        case UnifiedOpcode::OP_SB: m_exec.cpuExec()->executeMemoryOperation<P::STORE, int8_t>(data); break;
        case UnifiedOpcode::OP_SH: m_exec.cpuExec()->executeMemoryOperation<P::STORE, int16_t>(data); break;
        case UnifiedOpcode::OP_SW: m_exec.cpuExec()->executeMemoryOperation<P::STORE, int32_t>(data); break;
        case UnifiedOpcode::OP_SD: m_exec.cpuExec()->executeMemoryOperation<P::STORE, int64_t>(data); break;

        // Arithmetic instructions
        case UnifiedOpcode::OP_ADD: [[fallthrough]]; // TODO overflow exception
        case UnifiedOpcode::OP_ADDU: m_exec.cpuExec()->executeBivariate(data, Func::ADD); break;
        case UnifiedOpcode::OP_ADDI: [[fallthrough]]; // TODO overflow exception
        case UnifiedOpcode::OP_ADDIU: m_exec.cpuExec()->executeBivariateImmediate<P::SIGN_EXTEND>(data, Func::ADD); break;
        case UnifiedOpcode::OP_SLT: m_exec.cpuExec()->executeBivariate(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_SLTU: m_exec.cpuExec()->executeBivariate<uint32_t>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_SLTI: m_exec.cpuExec()->executeBivariateImmediate<P::SIGN_EXTEND, int32_t>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_SLTIU: m_exec.cpuExec()->executeBivariateImmediate<P::ZERO_EXTEND, uint32_t>(data, Func::CMP_LT); break;
        case UnifiedOpcode::OP_SUB: [[fallthrough]]; // TODO overflow exception
        case UnifiedOpcode::OP_SUBU: m_exec.cpuExec()->executeBivariate(data, Func::SUB); break;
        case UnifiedOpcode::OP_AND: m_exec.cpuExec()->executeBivariate(data, Func::AND); break;
        case UnifiedOpcode::OP_ANDI: m_exec.cpuExec()->executeBivariateImmediate<P::ZERO_EXTEND>(data, Func::AND); break;
        case UnifiedOpcode::OP_OR: m_exec.cpuExec()->executeBivariate(data, Func::OR); break;
        case UnifiedOpcode::OP_ORI: m_exec.cpuExec()->executeBivariateImmediate<P::ZERO_EXTEND>(data, Func::OR); break;
        case UnifiedOpcode::OP_XOR: m_exec.cpuExec()->executeBivariate(data, Func::XOR); break;
        case UnifiedOpcode::OP_XORI: m_exec.cpuExec()->executeBivariateImmediate<P::ZERO_EXTEND>(data, Func::XOR); break;
        case UnifiedOpcode::OP_NOR: m_exec.cpuExec()->executeBivariate(data, Func::NOR); break;

        // Shift instructions
        case UnifiedOpcode::OP_SLL: m_exec.cpuExec()->executeShift<P::WORD, P::LEFT, P::LOGICAL>(data); break;
        case UnifiedOpcode::OP_SRL: m_exec.cpuExec()->executeShift<P::WORD, P::RIGHT, P::LOGICAL>(data); break;
        case UnifiedOpcode::OP_SRA: m_exec.cpuExec()->executeShift<P::WORD, P::RIGHT, P::ARITHMETIC>(data); break;
        case UnifiedOpcode::OP_SLLV: m_exec.cpuExec()->executeShift<P::WORD, P::LEFT, P::LOGICAL, P::VARIABLE>(data); break;
        case UnifiedOpcode::OP_SRLV: m_exec.cpuExec()->executeShift<P::WORD, P::RIGHT, P::LOGICAL, P::VARIABLE>(data); break;
        case UnifiedOpcode::OP_SRAV: m_exec.cpuExec()->executeShift<P::WORD, P::RIGHT, P::ARITHMETIC, P::VARIABLE>(data); break;

        // Vector instructions
        case UnifiedOpcode::OP_LBV: m_exec.executeLoadStore<P::LOADV, 1uz>(data); break;
        case UnifiedOpcode::OP_LSV: m_exec.executeLoadStore<P::LOADV, 2uz>(data); break;
        case UnifiedOpcode::OP_LLV: m_exec.executeLoadStore<P::LOADV, 4uz>(data); break;
        case UnifiedOpcode::OP_LDV: m_exec.executeLoadStore<P::LOADV, 8uz>(data); break;
        case UnifiedOpcode::OP_SBV: m_exec.executeLoadStore<P::STOREV, 1uz>(data); break;
        case UnifiedOpcode::OP_SSV: m_exec.executeLoadStore<P::STOREV, 2uz>(data); break;
        case UnifiedOpcode::OP_SLV: m_exec.executeLoadStore<P::STOREV, 4uz>(data); break;
        case UnifiedOpcode::OP_SDV: m_exec.executeLoadStore<P::STOREV, 8uz>(data); break;
        case UnifiedOpcode::OP_LPV: m_exec.executeLoadStorePacked<P::LOADV, P::SIGNED>(data); break;
        case UnifiedOpcode::OP_LUV: m_exec.executeLoadStorePacked<P::LOADV, P::UNSIGNED>(data); break;
        case UnifiedOpcode::OP_SPV: m_exec.executeLoadStorePacked<P::STOREV, P::SIGNED>(data); break;
        case UnifiedOpcode::OP_SUV: m_exec.executeLoadStorePacked<P::STOREV, P::UNSIGNED>(data); break;
        case UnifiedOpcode::OP_LQV: m_exec.executeLoadStoreQuad<P::LOADV>(data); break;
        case UnifiedOpcode::OP_SQV: m_exec.executeLoadStoreQuad<P::STOREV>(data); break;

        case UnifiedOpcode::OP_VADD: m_exec.executeBivariateWithCarryIn<P::ACCUM_ZERO_EXT, P::CLAMP_SIGNED>(data, Func::ADD); break;
        case UnifiedOpcode::OP_VSUB: m_exec.executeBivariateWithCarryIn<P::ACCUM_ZERO_EXT, P::CLAMP_SIGNED>(data, Func::SUB); break;
        case UnifiedOpcode::OP_VADDC: m_exec.executeBivariate<P::ACCUM_ZERO_EXT>(data, Func::ADD, [](uint32_t sum) { return sum >> 16; }, [](auto _) { return 0; }); break;
        case UnifiedOpcode::OP_VSUBC: m_exec.executeBivariate<P::ACCUM_ZERO_EXT>(data, Func::SUB, [](uint32_t sum) { return sum >> 16; }, [](uint32_t sum) { return (sum & 0x1FFFF) != 0; }); break;

        case UnifiedOpcode::OP_VMADL: m_exec.executeMultiply<P::CLAMP_SIGNED, P::UNSIGNED, P::UNSIGNED, P::ACCUM_ADD, P::Shift(-16)>(data); break;
        case UnifiedOpcode::OP_VMUDL: m_exec.executeMultiply<P::CLAMP_SIGNED, P::UNSIGNED, P::UNSIGNED, P::ACCUM_SET, P::Shift(-16)>(data); break;
        case UnifiedOpcode::OP_VMADN: m_exec.executeMultiply<P::CLAMP_SIGNED, P::UNSIGNED, P::SIGNED, P::ACCUM_ADD>(data); break;
        case UnifiedOpcode::OP_VMUDN: m_exec.executeMultiply<P::CLAMP_SIGNED, P::UNSIGNED, P::SIGNED, P::ACCUM_SET>(data); break;
        case UnifiedOpcode::OP_VMADM: m_exec.executeMultiply<P::CLAMP_SIGNED, P::SIGNED, P::UNSIGNED, P::ACCUM_ADD>(data); break;
        case UnifiedOpcode::OP_VMUDM: m_exec.executeMultiply<P::CLAMP_SIGNED, P::SIGNED, P::UNSIGNED, P::ACCUM_SET>(data); break;
        case UnifiedOpcode::OP_VMADH: m_exec.executeMultiply<P::CLAMP_SIGNED, P::SIGNED, P::SIGNED, P::ACCUM_ADD, P::Shift(16)>(data); break;
        case UnifiedOpcode::OP_VMUDH: m_exec.executeMultiply<P::CLAMP_SIGNED, P::SIGNED, P::SIGNED, P::ACCUM_SET, P::Shift(16)>(data); break;

        case UnifiedOpcode::OP_VMULF: m_exec.executeMultiply<P::CLAMP_SIGNED, P::SIGNED, P::SIGNED, P::ACCUM_SET, P::Shift(1), P::ROUND>(data); break;
        case UnifiedOpcode::OP_VMULU: m_exec.executeMultiply<P::CLAMP_UNSIGNED, P::SIGNED, P::SIGNED, P::ACCUM_SET, P::Shift(1), P::ROUND>(data); break;
        case UnifiedOpcode::OP_VMACF: m_exec.executeMultiply<P::CLAMP_SIGNED, P::SIGNED, P::SIGNED, P::ACCUM_ADD, P::Shift(1)>(data); break;
        case UnifiedOpcode::OP_VMACU: m_exec.executeMultiply<P::CLAMP_UNSIGNED, P::SIGNED, P::SIGNED, P::ACCUM_ADD, P::Shift(1)>(data); break;

        case UnifiedOpcode::OP_VAND: m_exec.executeBivariate<P::ACCUM_ZERO_EXT>(data, Func::AND); break;
        case UnifiedOpcode::OP_VNAND: m_exec.executeBivariate<P::ACCUM_ZERO_EXT>(data, Func::NAND); break;
        case UnifiedOpcode::OP_VOR: m_exec.executeBivariate<P::ACCUM_ZERO_EXT>(data, Func::OR); break;
        case UnifiedOpcode::OP_VNOR: m_exec.executeBivariate<P::ACCUM_ZERO_EXT>(data, Func::NOR); break;
        case UnifiedOpcode::OP_VXOR: m_exec.executeBivariate<P::ACCUM_ZERO_EXT>(data, Func::XOR); break;
        case UnifiedOpcode::OP_VNXOR: m_exec.executeBivariate<P::ACCUM_ZERO_EXT>(data, Func::NXOR); break;

        case UnifiedOpcode::OP_VMOV: m_exec.executeSingleLane<P::ACCUM_ZERO_EXT>(data, Func::NOP); break;

        case UnifiedOpcode::OP_VLT:
            m_exec.executeSelectCompare(data, [](uint16_t vs, uint16_t vt, bool vco, bool vce) {
                return (vs < vt) | (vs == vt && vco && !vce);
            });
            break;
        case UnifiedOpcode::OP_VNE:
            m_exec.executeSelectCompare(data, [](uint16_t vs, uint16_t vt, bool _, bool vce) {
                return (vs < vt) | (vs > vt) | (vs == vt && !vce);
            });
            break;
        case UnifiedOpcode::OP_VEQ:
            m_exec.executeSelectCompare(data, [](uint16_t vs, uint16_t vt, bool _, bool vce) {
                return vs == vt && vce;
            });
            break;
        case UnifiedOpcode::OP_VGE:
            m_exec.executeSelectCompare(data, [](uint16_t vs, uint16_t vt, bool vco, bool vce) {
                return (vs > vt) | (vs == vt && ((!vco) | vce));
            });
            break;
        case UnifiedOpcode::OP_VCH: m_exec.executeSelectClipHigh(data); break;
        case UnifiedOpcode::OP_VCL: m_exec.executeSelectClipLow(data); break;
        case UnifiedOpcode::OP_VCR: m_exec.executeSelectCrimpLow(data); break;
        case UnifiedOpcode::OP_VMRG: m_exec.executeSelectMerge(data); break;
        case UnifiedOpcode::OP_VSAR: m_exec.executeReadAccumulators(data); break;
        case UnifiedOpcode::OP_VRCPH: [[fallthrough]];
        case UnifiedOpcode::OP_VRSQH: m_exec.executeReciprocalHigh(data); break;
        case UnifiedOpcode::OP_VRCPL: m_exec.executeReciprocalLow<P::RECIP>(data); break;
        case UnifiedOpcode::OP_VRSQL: m_exec.executeReciprocalLow<P::RECIP_SQRT>(data); break;

        // Coprocessor instructions
        case UnifiedOpcode::OP_MFCz: {
            auto cp = inst.getCoprocessor();
            if (cp != 0) throw Util::Error("Unsupported instruction on coprocessor {}", cp);
            auto ops = std::bit_cast<TypeR>(data);
            m_gprs.writeGpr(ops.rt, m_control->readRegister(ops.rd));

            break;
        }
        case UnifiedOpcode::OP_MTCz: {
            auto cp = inst.getCoprocessor();
            if (cp != 0) throw Util::Error("Unsupported instruction on coprocessor {}", cp);
            auto ops = std::bit_cast<TypeR>(data);
            m_control->writeRegister(ops.rd, m_gprs.readGpr(ops.rt));
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

        case UnifiedOpcode::OP_BREAK:
            if (m_control->getIntBreak()) {
                m_mipsInterface->setInterrupt<^^Interfaces::MI_INTERRUPT::sp>(true);
            }
            halt();
            break;
        default:
            dumpIMem("rsp_imem.txt");
            throw Util::Error("RSP unimplemented instruction @ PC {:#05x}: {} ({:#08x})", m_gprs.readPc(), inst, data);
    }

    m_gprs.advancePc();

    if (m_control->getSingleStep()) {
        halt();
    }
}

auto RSP::halt() -> void {
    // save PC when halting
    m_control->setHalt(true);
    m_control->setPc(m_gprs.readPc());
}

auto RSP::dumpIMem(std::filesystem::path file) -> void {
    auto romDumper = Util::Logger(file);
    romDumper.setLevel(Level::MAX);
    for (auto i = 0uz; i < 0x1000; i += 4) {
        const auto word = m_memory->readPhysical<uint32_t>(RSP_IMEM_BASE + i);
        romDumper.print("{:#05x}: {}", i, ISA::Instruction(word));
    }
    romDumper.flush();
}

} // namespace RSP
