module;
#include <util/defines.hpp>
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

        constexpr auto getCoprocessor() const -> uint32_t {
            return (data >> 26) & 0b11;
        }

        constexpr auto isCOP1() const {
            using namespace Opcodes;
            const auto op              = static_cast<uint32_t>(opcode);
            const auto isCOP1          = (UnifiedOpcodeBase::CP1_BASE <= op && op < UnifiedOpcodeBase::COP2_LOAD_BASE);
            const auto isStoreLoadCOP1 = opcode == UnifiedOpcode::OP_LWC1 || opcode == UnifiedOpcode::OP_SWC1 || opcode == UnifiedOpcode::OP_LDC1 || opcode == UnifiedOpcode::OP_SDC1;
            const auto isCOPz_COP1     = (UnifiedOpcodeBase::COPz_rs_BASE <= op && op < UnifiedOpcodeBase::CP0_BASE) && (getCoprocessor() == 1);
            return isCOP1 || isStoreLoadCOP1 || isCOPz_COP1;
        }
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

        case OPCODE::OP_COP0: [[fallthrough]];
        case OPCODE::OP_COP1: [[fallthrough]];
        case OPCODE::OP_COP2: {
            if (instR.rs < 16) {
                if (Util::scopedEnumCast<COPz_rs>(instR.rs) == COPz_rs::OP_BC) {
                    unifiedOpcode = UnifiedOpcodeBase::COPz_rt_BASE + instR.rt;
                } else {
                    unifiedOpcode = UnifiedOpcodeBase::COPz_rs_BASE + instR.rs;
                }
            } else {
                if (opcode == OPCODE::OP_COP0) {
                    unifiedOpcode = UnifiedOpcodeBase::CP0_BASE + instR.func;
                } else if (opcode == OPCODE::OP_COP1) {
                    unifiedOpcode = UnifiedOpcodeBase::CP1_BASE + instR.func;
                } else {
                    unifiedOpcode = UnifiedOpcodeBase::CP2_BASE + instR.func;
                }
            }
            break;
        }
        case OPCODE::OP_LWC2: unifiedOpcode = UnifiedOpcodeBase::COP2_LOAD_BASE + instR.rd; break;
        case OPCODE::OP_SWC2: unifiedOpcode = UnifiedOpcodeBase::COP2_STORE_BASE + instR.rd; break;
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
                constexpr auto aliasedOperandType = std::meta::extract<std::meta::info>(anns.front());
                constexpr auto operandType        = std::meta::dealias(aliasedOperandType);

                constexpr static auto operands = std::define_static_array(
                    std::meta::template_arguments_of(operandType) | std::views::drop(1));

                template for (constexpr auto op : operands) {
                    // extract the OpFormat annotation for the operand
                    constexpr auto opFmt   = std::meta::extract<std::meta::info>(Util::annotationOf([:op:]));
                    constexpr auto fmtStr  = [:std::meta::template_arguments_of(opFmt)[0]:];
                    constexpr auto fmtType = std::meta::template_arguments_of(opFmt)[1];

                    const auto instData = std::bit_cast<typename[:operandType:] ::InstType>(inst.data);

                    const uint32_t opValue = instData.[:[:op:]:];

                    // CPz registers have different names
                    constexpr auto opName = std::meta::identifier_of([:op:]);
                    if constexpr (std::meta::display_string_of(aliasedOperandType).contains("CPU_CPMove")) {
                        if (opName == "rd") {
                            if (inst.getCoprocessor() == 1) {
                                result.push_back(std::format("$f{}", opValue));
                            } else {
                                result.push_back(std::format("${}", opValue));
                            }
                            continue;
                        }
                    }
                    if constexpr (std::meta::display_string_of(aliasedOperandType).contains("CP1_LoadStore")) {
                        if (opName == "rt") {
                            if (inst.getCoprocessor() == 1) {
                                result.push_back(std::format("$f{}", opValue));
                            } else {
                                result.push_back(std::format("${}", opValue));
                            }
                            continue;
                        }
                    }

                    // RSP Load/Store elements are byte indices
                    if constexpr (std::meta::display_string_of(aliasedOperandType).contains("RSP_LoadStore")) {
                        if (opName == "vtElem") {
                            result.push_back(std::format("[{}]", opValue));
                            continue;
                        }
                        if (opName == "imm") { // offsets must be shifted
                            const auto opcode = inst.opcode;
                            const auto shift  = [opcode] {
                                switch (opcode) {
                                    case UnifiedOpcode::OP_LBV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SBV: return 0;
                                    case UnifiedOpcode::OP_LSV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SSV: return 1;
                                    case UnifiedOpcode::OP_LLV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SLV: return 2;
                                    case UnifiedOpcode::OP_LPV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SPV: [[fallthrough]];
                                    case UnifiedOpcode::OP_LUV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SUV: [[fallthrough]];
                                    case UnifiedOpcode::OP_LDV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SDV: return 3;
                                    case UnifiedOpcode::OP_LQV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SQV: [[fallthrough]];
                                    case UnifiedOpcode::OP_LRV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SRV: [[fallthrough]];
                                    case UnifiedOpcode::OP_LHV: [[fallthrough]];
                                    case UnifiedOpcode::OP_SHV: return 4;
                                    default: throw Util::Error("Unexpected RSP Load/Store opcode {}", opcode);
                                }
                            }();
                            result.push_back(std::format("{:#x}", opValue << shift));
                            continue;
                        }
                    }

                    // vformat does runtime format-string checking, avoiding a compiler
                    // limitation checking consteval format strings produced via reflection
                    const auto arg   = static_cast<typename[:fmtType:]>(instData.[:[:op:]:]);
                    const auto opStr = std::vformat(std::string_view{[:fmtStr:]}, std::make_format_args(arg));
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

    // format the opcode
    auto opcode = Util::enumName(inst.opcode);
    if (opcode.has_value()) {
        // insert coprocessor number
        if (auto it = opcode->find("z"); it != opcode->npos) {
            const auto cpIndex = inst.getCoprocessor();
            opcode->replace(it, 1, std::format("{}", cpIndex));
        }
        // format CP1 instructions
        if (auto it = opcode->find("FMT"); it != opcode->npos) {
            const auto fmtIndex = (inst.data >> 21) & 0b11111;
            opcode->replace(it, 3, std::format("{}", static_cast<CP1_FORMAT>(fmtIndex)));
            std::replace(opcode->begin(), opcode->end(), '_', '.');
        }
        instStr += std::format("{:8}", std::string_view(*opcode).substr(3));
    } else {
        instStr += std::format("UNKNOWN INSTRUCTION (" HEXFMT32 ")", inst.data);
    }

    // format the operands
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

constexpr Instruction::Instruction(uint32_t bits) : opcode(Impl::opcodeFor(bits)), data(bits) {}

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