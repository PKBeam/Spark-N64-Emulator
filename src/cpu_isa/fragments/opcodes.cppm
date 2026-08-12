export module ISA:Opcodes;

import std;
import :Operands;

// clang-format off
export namespace Opcodes {

enum class OPCODE : uint8_t {
    OP_SPECIAL = 0b000000,
    OP_REGIMM  = 0b000001,
    OP_J       [[=^^ISA::CPUOpType::Jump]]      = 0b000010,
    OP_JAL     [[=^^ISA::CPUOpType::Jump]]      = 0b000011,
    OP_BEQ     [[=^^ISA::CPUOpType::Branch2]]   = 0b000100,
    OP_BNE     [[=^^ISA::CPUOpType::Branch2]]   = 0b000101,
    OP_BLEZ    [[=^^ISA::CPUOpType::Branch1]]   = 0b000110,
    OP_BGTZ    [[=^^ISA::CPUOpType::Branch1]]   = 0b000111,
    OP_ADDI    [[=^^ISA::CPUOpType::AluImm]]    = 0b001000,
    OP_ADDIU   [[=^^ISA::CPUOpType::AluImm]]    = 0b001001,
    OP_SLTI    [[=^^ISA::CPUOpType::AluImm]]    = 0b001010,
    OP_SLTIU   [[=^^ISA::CPUOpType::AluImm]]    = 0b001011,
    OP_ANDI    [[=^^ISA::CPUOpType::AluImm]]    = 0b001100,
    OP_ORI     [[=^^ISA::CPUOpType::AluImm]]    = 0b001101,
    OP_XORI    [[=^^ISA::CPUOpType::AluImm]]    = 0b001110,
    OP_LUI     [[=^^ISA::CPUOpType::AluImmLoad]] = 0b001111,
    OP_COP0    = 0b010000,
    OP_COP1    = 0b010001,
    OP_COP2    = 0b010010,
    OP_BEQL    [[=^^ISA::CPUOpType::Branch2]]   = 0b010100,
    OP_BNEL    [[=^^ISA::CPUOpType::Branch2]]   = 0b010101,
    OP_BLEZL   [[=^^ISA::CPUOpType::Branch2]]   = 0b010110,
    OP_BGTZL   [[=^^ISA::CPUOpType::Branch2]]   = 0b010111,
    OP_DADDI   [[=^^ISA::CPUOpType::AluImm]]    = 0b011000,
    OP_DADDIU  [[=^^ISA::CPUOpType::AluImm]]    = 0b011001,
    OP_LDL     [[=^^ISA::CPUOpType::LoadStore]] = 0b011010,
    OP_LDR     [[=^^ISA::CPUOpType::LoadStore]] = 0b011011,
    OP_LB      [[=^^ISA::CPUOpType::LoadStore]] = 0b100000,
    OP_LH      [[=^^ISA::CPUOpType::LoadStore]] = 0b100001,
    OP_LWL     [[=^^ISA::CPUOpType::LoadStore]] = 0b100010,
    OP_LW      [[=^^ISA::CPUOpType::LoadStore]] = 0b100011,
    OP_LBU     [[=^^ISA::CPUOpType::LoadStore]] = 0b100100,
    OP_LHU     [[=^^ISA::CPUOpType::LoadStore]] = 0b100101,
    OP_LWR     [[=^^ISA::CPUOpType::LoadStore]] = 0b100110,
    OP_LWU     [[=^^ISA::CPUOpType::LoadStore]] = 0b100111,
    OP_SB      [[=^^ISA::CPUOpType::LoadStore]] = 0b101000,
    OP_SH      [[=^^ISA::CPUOpType::LoadStore]] = 0b101001,
    OP_SWL     [[=^^ISA::CPUOpType::LoadStore]] = 0b101010,
    OP_SW      [[=^^ISA::CPUOpType::LoadStore]] = 0b101011,
    OP_SDL     [[=^^ISA::CPUOpType::LoadStore]] = 0b101100,
    OP_SDR     [[=^^ISA::CPUOpType::LoadStore]] = 0b101101,
    OP_SWR     [[=^^ISA::CPUOpType::LoadStore]] = 0b101110,
    OP_CACHE   [[=^^ISA::CPUOpType::LoadStore]] = 0b101111,
    OP_LL      [[=^^ISA::CPUOpType::LoadStore]] = 0b110000,
    OP_LWC1    = 0b110001,
    OP_LWC2    = 0b110010,
    OP_LLD     [[=^^ISA::CPUOpType::LoadStore]] = 0b110100,
    OP_LDC1    = 0b110101,
    OP_LDC2    = 0b110110,
    OP_LD      [[=^^ISA::CPUOpType::LoadStore]] = 0b110111,
    OP_SC      [[=^^ISA::CPUOpType::LoadStore]] = 0b111000,
    OP_SWC1    = 0b111001,
    OP_SWC2    = 0b111010,
    OP_SCD     [[=^^ISA::CPUOpType::LoadStore]] = 0b111100,
    OP_SDC1    = 0b111101,
    OP_SDC2    = 0b111110,
    OP_SD      [[=^^ISA::CPUOpType::LoadStore]] = 0b111111,
};

enum class SPECIAL : uint8_t {
    OP_SLL     [[=^^ISA::CPUOpType::Shift]]       = 0b000000,
    OP_SRL     [[=^^ISA::CPUOpType::Shift]]       = 0b000010,
    OP_SRA     [[=^^ISA::CPUOpType::Shift]]       = 0b000011,
    OP_SLLV    [[=^^ISA::CPUOpType::ShiftVar]]    = 0b000100,
    OP_SRLV    [[=^^ISA::CPUOpType::ShiftVar]]    = 0b000110,
    OP_SRAV    [[=^^ISA::CPUOpType::ShiftVar]]    = 0b000111,
    OP_JR      [[=^^ISA::CPUOpType::JumpReg]]     = 0b001000,
    OP_JALR    [[=^^ISA::CPUOpType::JumpLinkReg]] = 0b001001,
    OP_SYSCALL [[=^^ISA::CPUOpType::Special]]     = 0b001100,
    OP_BREAK   [[=^^ISA::CPUOpType::Special]]     = 0b001101,
    OP_SYNC    [[=^^ISA::CPUOpType::Special]]     = 0b001111,
    OP_MFHI    [[=^^ISA::CPUOpType::MoveFrom]]    = 0b010000,
    OP_MTHI    [[=^^ISA::CPUOpType::MoveTo]]      = 0b010001,
    OP_MFLO    [[=^^ISA::CPUOpType::MoveFrom]]    = 0b010010,
    OP_MTLO    [[=^^ISA::CPUOpType::MoveTo]]      = 0b010011,
    OP_DSLLV   [[=^^ISA::CPUOpType::ShiftVar]]    = 0b010100,
    OP_DSRLV   [[=^^ISA::CPUOpType::ShiftVar]]    = 0b010110,
    OP_DSRAV   [[=^^ISA::CPUOpType::ShiftVar]]    = 0b010111,
    OP_MULT    [[=^^ISA::CPUOpType::MulDiv]]      = 0b011000,
    OP_MULTU   [[=^^ISA::CPUOpType::MulDiv]]      = 0b011001,
    OP_DIV     [[=^^ISA::CPUOpType::MulDiv]]      = 0b011010,
    OP_DIVU    [[=^^ISA::CPUOpType::MulDiv]]      = 0b011011,
    OP_DMULT   [[=^^ISA::CPUOpType::MulDiv]]      = 0b011100,
    OP_DMULTU  [[=^^ISA::CPUOpType::MulDiv]]      = 0b011101,
    OP_DDIV    [[=^^ISA::CPUOpType::MulDiv]]      = 0b011110,
    OP_DDIVU   [[=^^ISA::CPUOpType::MulDiv]]      = 0b011111,
    OP_ADD     [[=^^ISA::CPUOpType::ThreeOp]]     = 0b100000,
    OP_ADDU    [[=^^ISA::CPUOpType::ThreeOp]]     = 0b100001,
    OP_SUB     [[=^^ISA::CPUOpType::ThreeOp]]     = 0b100010,
    OP_SUBU    [[=^^ISA::CPUOpType::ThreeOp]]     = 0b100011,
    OP_AND     [[=^^ISA::CPUOpType::ThreeOp]]     = 0b100100,
    OP_OR      [[=^^ISA::CPUOpType::ThreeOp]]     = 0b100101,
    OP_XOR     [[=^^ISA::CPUOpType::ThreeOp]]     = 0b100110,
    OP_NOR     [[=^^ISA::CPUOpType::ThreeOp]]     = 0b100111,
    OP_SLT     [[=^^ISA::CPUOpType::ThreeOp]]     = 0b101010,
    OP_SLTU    [[=^^ISA::CPUOpType::ThreeOp]]     = 0b101011,
    OP_DADD    [[=^^ISA::CPUOpType::ThreeOp]]     = 0b101100,
    OP_DADDU   [[=^^ISA::CPUOpType::ThreeOp]]     = 0b101101,
    OP_DSUB    [[=^^ISA::CPUOpType::ThreeOp]]     = 0b101110,
    OP_DSUBU   [[=^^ISA::CPUOpType::ThreeOp]]     = 0b101111,
    OP_TGE     [[=^^ISA::CPUOpType::Trap]]        = 0b110000,
    OP_TGEU    [[=^^ISA::CPUOpType::Trap]]        = 0b110001,
    OP_TLT     [[=^^ISA::CPUOpType::Trap]]        = 0b110010,
    OP_TLTU    [[=^^ISA::CPUOpType::Trap]]        = 0b110011,
    OP_TEQ     [[=^^ISA::CPUOpType::Trap]]        = 0b110100,
    OP_TNE     [[=^^ISA::CPUOpType::Trap]]        = 0b110110,
    OP_DSLL    [[=^^ISA::CPUOpType::Shift]]       = 0b111000,
    OP_DSRL    [[=^^ISA::CPUOpType::Shift]]       = 0b111010,
    OP_DSRA    [[=^^ISA::CPUOpType::Shift]]       = 0b111011,
    OP_DSLL32  [[=^^ISA::CPUOpType::Shift]]       = 0b111100,
    OP_DSRL32  [[=^^ISA::CPUOpType::Shift]]       = 0b111110,
    OP_DSRA32  [[=^^ISA::CPUOpType::Shift]]       = 0b111111,
};

enum class REGIMM_rt : uint8_t {
    OP_BLTZ    [[=^^ISA::CPUOpType::Branch1]] = 0b000000,
    OP_BGEZ    [[=^^ISA::CPUOpType::Branch1]] = 0b000001,
    OP_BLTZL   [[=^^ISA::CPUOpType::Branch1]] = 0b000010,
    OP_BGEZL   [[=^^ISA::CPUOpType::Branch1]] = 0b000011,
    OP_TGEI    [[=^^ISA::CPUOpType::TrapImm]] = 0b001000,
    OP_TGEIU   [[=^^ISA::CPUOpType::TrapImm]] = 0b001001,
    OP_TLTI    [[=^^ISA::CPUOpType::TrapImm]] = 0b001010,
    OP_TLTIU   [[=^^ISA::CPUOpType::TrapImm]] = 0b001011,
    OP_TEQI    [[=^^ISA::CPUOpType::TrapImm]] = 0b001100,
    OP_TNEI    [[=^^ISA::CPUOpType::TrapImm]] = 0b001110,
    OP_BLTZAL  [[=^^ISA::CPUOpType::Branch1]] = 0b010000,
    OP_BGEZAL  [[=^^ISA::CPUOpType::Branch1]] = 0b010001,
    OP_BLTZALL [[=^^ISA::CPUOpType::Branch1]] = 0b010010,
    OP_BGEZALL [[=^^ISA::CPUOpType::Branch1]] = 0b010011,
};

// two high bits = 'z' (coprocessor number)
enum class COPz_rs : uint8_t {
    OP_MFCz  [[=^^ISA::CPUOpType::CP0Move]] = 0b00000,
    OP_DMFCz [[=^^ISA::CPUOpType::CP0Move]] = 0b00001,
    OP_CFCz  [[=^^ISA::CPUOpType::CP0Move]] = 0b00010,
    OP_MTCz  [[=^^ISA::CPUOpType::CP0Move]] = 0b00100,
    OP_DMTCz [[=^^ISA::CPUOpType::CP0Move]] = 0b00101,
    OP_CTCz  [[=^^ISA::CPUOpType::CP0Move]] = 0b00110,
    OP_BC    [[=^^ISA::CPUOpType::CP0Move]] = 0b01000,
};

enum class COPz_rt : uint8_t {
    OP_BCzF  [[=^^ISA::CPUOpType::TypeI_Imm]] = 0b00000,
    OP_BCzT  [[=^^ISA::CPUOpType::TypeI_Imm]] = 0b00001,
    OP_BCzFL [[=^^ISA::CPUOpType::TypeI_Imm]] = 0b00010,
    OP_BCzTL [[=^^ISA::CPUOpType::TypeI_Imm]] = 0b00100,
};

enum class CP0 : uint8_t {
    OP_TLBR  = 0b000001,
    OP_TLBWI = 0b000010,
    OP_TLBWR = 0b000110,
    OP_TLBP  = 0b001000,
    OP_ERET  = 0b011000,
};

// clang-format on

using Opcode = std::variant<OPCODE, SPECIAL, REGIMM_rt, COPz_rs, COPz_rt, CP0>;

template <typename E1, typename E2>
    requires( // todo replace with reflection helper
        (std::is_same_v<E1, OPCODE> || std::is_same_v<E1, SPECIAL> || std::is_same_v<E1, REGIMM_rt> || std::is_same_v<E1, COPz_rs> || std::is_same_v<E1, COPz_rt> || std::is_same_v<E1, CP0>) &&
        (std::is_same_v<E2, OPCODE> || std::is_same_v<E2, SPECIAL> || std::is_same_v<E2, REGIMM_rt> || std::is_same_v<E2, COPz_rs> || std::is_same_v<E2, COPz_rt> || std::is_same_v<E2, CP0>))
constexpr auto operator==(E1 e1, E2 e2) -> bool {
    if constexpr (std::is_same_v<E1, E2>) {
        return e1 == e2;
    } else {
        return false;
    }
}
} // namespace Opcodes
