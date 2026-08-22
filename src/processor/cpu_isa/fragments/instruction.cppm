export module ISA:Instruction;

import std;

import :InstructionData;
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

        case OPCODE::OP_COP1: [[fallthrough]];
        case OPCODE::OP_COP2: {
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

        default:
            unifiedOpcode = UnifiedOpcodeBase::OPCODE_BASE + static_cast<uint32_t>(opcode);
            break;
    }
    return static_cast<UnifiedOpcode>(unifiedOpcode);
}

constexpr auto formatOps(Instruction inst) -> std::vector<std::string> {
    auto result = std::vector<std::string>{};

    template for (constexpr auto e : Util::staticEnumeratorsOf(^^Opcodes::UnifiedOpcode)) {
        if (inst.opcode == std::meta::extract<Opcodes::UnifiedOpcode>(e)) {
            if constexpr (constexpr auto anns = Util::staticAnnotationsOf(e); !anns.empty()) {
                constexpr auto operandType = Util::dealiasedTypeOf(anns.front());

                constexpr static auto args = std::define_static_array(
                    std::meta::template_arguments_of(operandType) | std::views::drop(1));

                template for (constexpr auto a : args) {
                    auto instData = std::bit_cast<typename[:operandType:] ::InstType>(inst.data);
                    // gcc bug prevents usage of instData.[:a:]
                    // use this as workaround for now
                    constexpr auto name = std::meta::identifier_of([:a:]);
                    if constexpr (name == "rt") {
                        result.push_back(std::format("{}", static_cast<ISA::CPU_REG>(instData.rt)));
                    } else if constexpr (name == "rs") {
                        result.push_back(std::format("{}", static_cast<ISA::CPU_REG>(instData.rs)));
                    } else if constexpr (name == "imm") {
                        result.push_back(std::format("{:#x}", static_cast<uint32_t>(instData.imm)));
                    } else if constexpr (name == "tgt") {
                        // shift left 2 and append high order 4 bits (always 0x8?)
                        result.push_back(std::format("0x8{:05x}", static_cast<uint32_t>(instData.tgt << 2)));
                    } else if constexpr (name == "rd") {
                        result.push_back(std::format("{}", static_cast<ISA::CPU_REG>(instData.rd)));
                    } else if constexpr (name == "sa") {
                        result.push_back(std::format("{}", static_cast<uint32_t>(instData.sa)));
                    } else {
                        static_assert(false, name);
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
    auto ops = Impl::formatOps(inst);
    for (auto i = 0uz; i < ops.size(); ++i) {
        // "offset, base" format used in Load/Store insts
        if (i > 0 && ops[i - 1].substr(0, 2) == "0x") {
            instStr += std::format("({})", ops[i]);
        } else {
            if (i > 0) {
                instStr += std::format(", ");
            }
            instStr += std::format("{}", ops[i]);
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
