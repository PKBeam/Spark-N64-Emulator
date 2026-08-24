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
using AluImm        = Operands<CPU::TypeI, ^^CPU::TypeI::rt, ^^CPU::TypeI::rs, ^^CPU::TypeI::imm>;
using AluImmLoad    = Operands<CPU::TypeI, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm>;
using Branch1       = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::imm>;
using TrapImm       = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::imm>;
using LoadStore     = Operands<CPU::TypeI, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm, ^^CPU::TypeI::rs>;
using Branch2       = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm>;
using TypeI_Imm     = Operands<CPU::TypeI, ^^CPU::TypeI::imm>;
using TypeI_RsImm   = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::imm>;
using TypeI_RsRtImm = Operands<CPU::TypeI, ^^CPU::TypeI::rs, ^^CPU::TypeI::rt, ^^CPU::TypeI::imm>;
using Jump          = Operands<CPU::TypeJ, ^^CPU::TypeJ::tgt>;
using Special       = Operands<CPU::TypeR>;
using MoveFrom      = Operands<CPU::TypeR, ^^CPU::TypeR::rd>;
using MoveTo        = Operands<CPU::TypeR, ^^CPU::TypeR::rs>;
using JumpReg       = Operands<CPU::TypeR, ^^CPU::TypeR::rs>;
using JumpLinkReg   = Operands<CPU::TypeR, ^^CPU::TypeR::rs, ^^CPU::TypeR::rd>;
using MulDiv        = Operands<CPU::TypeR, ^^CPU::TypeR::rs, ^^CPU::TypeR::rt>;
using Trap          = Operands<CPU::TypeR, ^^CPU::TypeR::rs, ^^CPU::TypeR::rt>;
using Shift         = Operands<CPU::TypeR, ^^CPU::TypeR::rd, ^^CPU::TypeR::rt, ^^CPU::TypeR::sa>;
using ShiftVar      = Operands<CPU::TypeR, ^^CPU::TypeR::rd, ^^CPU::TypeR::rt, ^^CPU::TypeR::rs>;
using ThreeOp       = Operands<CPU::TypeR, ^^CPU::TypeR::rd, ^^CPU::TypeR::rs, ^^CPU::TypeR::rt>;
using CP0Move       = Operands<CPU::TypeR, ^^CPU::TypeR::rt, ^^CPU::TypeR::rd>;

using VecLoadStore  = Operands<RSP::TypeVI, ^^RSP::TypeVI::vt, ^^RSP::TypeVI::vtElem, ^^RSP::TypeVI::imm, ^^RSP::TypeVI::rs>;
using VecAlu        = Operands<RSP::TypeVR, ^^RSP::TypeVR::vd, ^^RSP::TypeVR::vs, ^^RSP::TypeVR::vt, ^^RSP::TypeVR::vtElem>;
using VecSingleLane = Operands<RSP::TypeVS, ^^RSP::TypeVS::vd, ^^RSP::TypeVS::vdElem, ^^RSP::TypeVS::vt, ^^RSP::TypeVS::vtElem>;
// clang-format on
} // namespace OpType

} // namespace ISA