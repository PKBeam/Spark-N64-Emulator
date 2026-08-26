export module ISA:Instruction;

import std;

import :InstructionTypes;
import :Opcodes;
import :Registers;

namespace ISA {

export {
    struct Instruction {
        Opcodes::UnifiedOpcode opcode;
        uint32_t               data;

        constexpr Instruction(uint32_t bits);
    };
}

// implementation

namespace Impl {
// Returns the fully-resolved opcode for an instruction
constexpr auto opcodeFor(uint32_t bits) -> Opcodes::UnifiedOpcode {
    using namespace Opcodes;
    const auto opcode = Util::scopedEnumCast<OPCODE>(bits >> 26);
    const auto instR  = std::bit_cast<CPU::TypeR>(bits);

    uint32_t unifiedOpcode = 0;
    switch (opcode) {
        case OPCODE::OP_SPECIAL:
            if (instR.func == 0 && instR.sa == 0) {
                unifiedOpcode = static_cast<uint32_t>(UnifiedOpcode::OP_NOP);
            } else {
                unifiedOpcode = UnifiedOpcodeBase::SPECIAL_BASE + instR.func;
            }
            break;

        case OPCODE::OP_REGIMM:
            unifiedOpcode = UnifiedOpcodeBase::REGIMM_rt_BASE + instR.rt;
            break;

        case OPCODE::OP_COP0:
            if ((bits >> 25) & 1) {
                unifiedOpcode = UnifiedOpcodeBase::CP0_BASE + (bits & 0x01FFFFFF);
                break;
            }
            [[fallthrough]];

        case OPCODE::OP_COP1: {
            if ((bits >> 25) & 1) { // COPz
                unifiedOpcode = static_cast<uint32_t>(UnifiedOpcode::OP_COPz);
                break;
            }
            const auto rsOpcode = Util::scopedEnumCast<COPz_rs>(instR.rs);
            if (rsOpcode == COPz_rs::OP_BC) {
                unifiedOpcode = UnifiedOpcodeBase::COPz_rt_BASE + instR.rt;
                break;
            } else {
                unifiedOpcode = UnifiedOpcodeBase::COPz_rs_BASE + instR.rs;
                break;
            }
        }
        case OPCODE::OP_LWC2: unifiedOpcode = UnifiedOpcodeBase::COP2_LOAD_BASE + instR.rd; break;
        case OPCODE::OP_SWC2: unifiedOpcode = UnifiedOpcodeBase::COP2_STORE_BASE + instR.rd; break;
        case OPCODE::OP_COP2:
            if ((bits >> 25) & 1) { // COPz
                unifiedOpcode = UnifiedOpcodeBase::COP2_VECTOR_BASE + static_cast<uint32_t>(instR.func);
            } else {
                unifiedOpcode = UnifiedOpcodeBase::COPz_rs_BASE + instR.rs;
            }
            break;
        default:
            unifiedOpcode = UnifiedOpcodeBase::OPCODE_BASE + static_cast<uint32_t>(opcode);
            break;
    }
    return static_cast<UnifiedOpcode>(unifiedOpcode);
}

constexpr auto formatOperands(Instruction inst) -> std::vector<std::string> {
    using namespace Opcodes;
    auto result = std::vector<std::string>{};

    template for (constexpr auto e : Util::staticEnumeratorsOf(^^Opcodes::UnifiedOpcode)) {
        if (inst.opcode == std::meta::extract<Opcodes::UnifiedOpcode>(e)) {
            if constexpr (constexpr auto anns = Util::staticAnnotationsOf(e); !anns.empty()) {
                constexpr auto operandType = Util::dealiasedTypeOf(anns.front());

                constexpr static auto operands = std::define_static_array(
                    std::meta::template_arguments_of(operandType) | std::views::drop(1));

                template for (constexpr auto op : operands) {
                    // extract the OpFormat annotation for the operand
                    constexpr auto opFmt   = std::meta::extract<std::meta::info>(Util::annotationOf([:op:]));
                    constexpr auto fmtStr  = [:std::meta::template_arguments_of(opFmt)[0]:];
                    constexpr auto fmtType = std::meta::template_arguments_of(opFmt)[1];

                    const auto instData = std::bit_cast<typename[:operandType:] ::InstType>(inst.data);

                    // TODO CP0 registers have different names
                    constexpr auto opName = std::meta::identifier_of([:op:]);
                    if constexpr (fmtType == (^^ISA::CPU_REG) && opName == "rd") {
                        if (inst.opcode == UnifiedOpcode::OP_MTCz || inst.opcode == UnifiedOpcode::OP_MFCz) {
                            const uint32_t opValue = instData.[:[:op:]:];
                            const auto     opStr   = std::format("$r{}", opValue);
                            result.push_back(opStr);
                            continue;
                        }
                    }

                    const auto opStr = std::format([:fmtStr:], static_cast<typename[:fmtType:]>(instData.[:[:op:]:]));
                    if (!opStr.empty()) {
                        result.push_back(opStr);
                    }
                };
                break;
            }
        }
    }
    return result;
}

constexpr auto formatInstruction(const Instruction& inst) -> std::string {
    // check for NOP
    if (inst.opcode == Opcodes::UnifiedOpcode::OP_SLL &&
        std::bit_cast<CPU::TypeR>(inst.data).sa == 0) {
        return std::format("{:8} ", "NOP");
    }

    auto instStr = std::string{};

    // print the opcode
    auto opcode = Util::enumName(inst.opcode);
    if (opcode.has_value()) {
        // insert coprocessor number
        if (auto it = opcode->find("z"); it != opcode->npos) {
            auto cpIndex = (inst.data >> 26) & 0b11;
            opcode->replace(it, 1, std::format("{}", cpIndex));
        }
        instStr += std::format("{:8}", std::string_view(*opcode).substr(3));
    } else {
        instStr += std::format("UNKNOWN INSTRUCTION (0x{:08X})", inst.data);
    }

    // print the operands
    auto ops = Impl::formatOperands(inst);
    for (auto i = 0uz; i < ops.size(); ++i) {
        const auto opIsVectorElem           = (ops[i].front() == '[' && ops[i].back() == ']');
        const auto opIsLoadStoreAddrReg     = i > 0 && !ops[i - 1].empty() && ops[i - 1].substr(0, 2) == "0x";
        const auto opIsEmptyLoadStoreOffset = i < ops.size() - 1 && !ops[i].empty() && ops[i] == "0x0";

        // operands that should never be comma-separated from their preceding operand
        if (i > 0 && !(opIsVectorElem || opIsLoadStoreAddrReg)) {
            instStr += std::format(", ");
        }
        if (!opIsEmptyLoadStoreOffset) { // never print zero-offset load/stores
            if (opIsLoadStoreAddrReg) {  // address registers should be parenthesised
                instStr += std::format("({})", ops[i]);
            } else {
                instStr += std::format("{}", ops[i]);
            }
        }
    }

    return instStr;
}
} // namespace Impl

constexpr Instruction::Instruction(uint32_t bits) {
    opcode = Impl::opcodeFor(bits);
    data   = bits;
}

} // namespace ISA

template <>
struct std::formatter<ISA::Instruction> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const ISA::Instruction& inst, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", ISA::Impl::formatInstruction(inst));
    }
};
