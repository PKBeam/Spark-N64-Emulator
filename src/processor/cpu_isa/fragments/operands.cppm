export module ISA:Operands;

import std;
import :InstructionTypes;

export namespace ISA {

template <typename I, std::meta::info... fields>
    requires(( // all Fields must be nonstatic data members of I
        std::ranges::contains(std::meta::nonstatic_data_members_of(^^I, std::meta::access_context::current()), fields) && ...))
struct Operands {
    using InstType = I;
    consteval Operands() : m_fields{fields...} {}
    std::array<std::meta::info, sizeof...(fields)> m_fields;
};

namespace OpType {
// clang-format off
using CPU_AluImm        = Operands<CPU::TypeI, ^^CPU::TypeI::rt, ^^CPU::TypeI::rs, ^^CPU::TypeI::imm>;
using CPU_AluImmLoad    = Operands<CPU::TypeI, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm>;
using CPU_Branch1       = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::imm>;
using CPU_TrapImm       = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::imm>;
using CPU_LoadStore     = Operands<CPU::TypeI, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm, ^^CPU::TypeI::rs>;
using CP1_LoadStore     = Operands<CPU::TypeI, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm, ^^CPU::TypeI::rs>;
using CPU_Branch2       = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm>;
using CPU_TypeI_Imm     = Operands<CPU::TypeI, ^^CPU::TypeI::imm>;
using CPU_TypeI_RsImm   = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::imm>;
using CPU_TypeI_RsRtImm = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm>;
using CPU_Jump          = Operands<CPU::TypeJ, ^^CPU::TypeJ::tgt>;
using CPU_Special       = Operands<CPU::TypeR>;
using CPU_MoveFrom      = Operands<CPU::TypeR, ^^CPU::TypeR::rd>;
using CPU_MoveTo        = Operands<CPU::TypeR, ^^CPU::TypeR::rs>;
using CPU_JumpReg       = Operands<CPU::TypeR, ^^CPU::TypeR::rs>;
using CPU_JumpLinkReg   = Operands<CPU::TypeR, ^^CPU::TypeR::rs, ^^CPU::TypeR::rd>;
using CPU_MulDiv        = Operands<CPU::TypeR, ^^CPU::TypeR::rs, ^^CPU::TypeR::rt>;
using CPU_Trap          = Operands<CPU::TypeR, ^^CPU::TypeR::rs, ^^CPU::TypeR::rt>;
using CPU_Shift         = Operands<CPU::TypeR, ^^CPU::TypeR::rd, ^^CPU::TypeR::rt, ^^CPU::TypeR::sa>;
using CPU_ShiftVar      = Operands<CPU::TypeR, ^^CPU::TypeR::rd, ^^CPU::TypeR::rt, ^^CPU::TypeR::rs>;
using CPU_Bivariate     = Operands<CPU::TypeR, ^^CPU::TypeR::rd, ^^CPU::TypeR::rs, ^^CPU::TypeR::rt>;
using CPU_CPMove        = Operands<CPU::TypeR, ^^CPU::TypeR::rt, ^^CPU::TypeR::rd>;

using RSP_LoadStore  = Operands<RSP::TypeVI, ^^RSP::TypeVI::vt, ^^RSP::TypeVI::vtElem, ^^RSP::TypeVI::imm, ^^RSP::TypeVI::rs>;
using RSP_Alu        = Operands<RSP::TypeVR, ^^RSP::TypeVR::vd, ^^RSP::TypeVR::vs, ^^RSP::TypeVR::vt, ^^RSP::TypeVR::vtElem>;
using RSP_SingleLane = Operands<RSP::TypeVS, ^^RSP::TypeVS::vd, ^^RSP::TypeVS::vdElem, ^^RSP::TypeVS::vt, ^^RSP::TypeVS::vtElem>;

using FPU_LoadStore   = Operands<FPU::TypeI, ^^FPU::TypeI::ft, ^^FPU::TypeI::imm, ^^FPU::TypeI::rs>;
using FPU_Transfer    = Operands<FPU::TypeO, ^^FPU::TypeO::rt, ^^FPU::TypeO::fs>;
using FPU_Bivariate   = Operands<FPU::TypeR, ^^FPU::TypeR::fd, ^^FPU::TypeR::fs, ^^FPU::TypeR::ft>;
using FPU_Univariate  = Operands<FPU::TypeR, ^^FPU::TypeR::fd, ^^FPU::TypeR::fs>;
using FPU_Compare     = Operands<FPU::TypeR, ^^FPU::TypeR::fs, ^^FPU::TypeR::ft>;
// clang-format on
} // namespace OpType

} // namespace ISA