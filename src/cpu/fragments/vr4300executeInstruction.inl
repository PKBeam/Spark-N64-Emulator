namespace Function {
constexpr auto ADD     = [](auto a, auto b) { return a + b; };
constexpr auto SUB     = [](auto a, auto b) { return a - b; };
constexpr auto AND     = [](auto a, auto b) { return a & b; };
constexpr auto OR      = [](auto a, auto b) { return a | b; };
constexpr auto NOR     = [](auto a, auto b) { return ~(a | b); };
constexpr auto XOR     = [](auto a, auto b) { return a ^ b; };
constexpr auto CMP_EQ  = [](auto a, auto b) { return a == b; };
constexpr auto CMP_NE  = [](auto a, auto b) { return a != b; };
constexpr auto CMP_LE  = [](auto a, auto b) { return a <= b; };
constexpr auto CMP_LT  = [](auto a, auto b) { return a < b; };
constexpr auto CMP_GE  = [](auto a, auto b) { return a >= b; };
constexpr auto CMP_NEZ = [](auto a, auto _) { return a != 0; };
constexpr auto CMP_LEZ = [](auto a, auto _) { return a <= 0; };
constexpr auto CMP_LTZ = [](auto a, auto _) { return a < 0; };
constexpr auto CMP_GEZ = [](auto a, auto _) { return a >= 0; };
} // namespace Function

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

    switch (op) {
        case UnifiedOpcode::OP_NOP: break;
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
            if (static_cast<uint32_t>(*pcJumpValue) == m_bootCallbackAddress) m_bootCallback();
            break;
        }
        case UnifiedOpcode::OP_JALR: {
            auto ops    = std::bit_cast<CPU::TypeR>(data);
            pcJumpValue = readGpr(ops.rs);
            writeGpr(ops.rd, m_regs.pc + 8);
            break;
        }
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
                m_memory->write<uint8_t>(vaddr + (4 - byte), thisByte);
            }
            break;
        }
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
        case UnifiedOpcode::OP_ADD: [[fallthrough]];
        case UnifiedOpcode::OP_ADDU: executeBivariate(data, Function::ADD); break;
        case UnifiedOpcode::OP_ADDI: [[fallthrough]];
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
                break;
            }
            throw Util::Error("Unsupported instruction on coprocessor {}", cp);
        }
        case UnifiedOpcode::OP_CTCz: {
            auto cp  = (data >> 26) & 0b11;
            auto ops = std::bit_cast<CPU::TypeR>(data);
            if (cp == 1 && ops.rd == 31) {
                writeGpr(ops.rt, 0);
                break;
            }
            throw Util::Error("Unsupported instruction on coprocessor {}", cp);
        }
        case UnifiedOpcode::OP_TLBR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWI: [[fallthrough]];
        case UnifiedOpcode::OP_TLBWR: [[fallthrough]];
        case UnifiedOpcode::OP_TLBP: [[fallthrough]];
        case UnifiedOpcode::OP_CACHE: break;
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
        default: throw Util::Error("Unimplemented instruction @ PC 0x{:08x}: {}", m_regs.pc, inst);
    }

    if (op == UnifiedOpcode::OP_BGEZAL) {
        auto ops = std::bit_cast<CPU::TypeI>(data);
        if (ops.rs == static_cast<uint32_t>(CPU_REG::zero) && static_cast<int16_t>(ops.imm) == -1 &&
            m_memory->read<uint32_t>(m_regs.pc - 4) == 0) {
            throw Util::Error("Detected infinite looping BGEZAL->NOP, likely boot checksum fail.");
        }
    }
    if (m_delaySlotPc) {
        writePc(*m_delaySlotPc);
        m_delaySlotPc.reset();
    } else
        m_regs.pc += 4;
    if (pcJumpValue) m_delaySlotPc = pcJumpValue;
}