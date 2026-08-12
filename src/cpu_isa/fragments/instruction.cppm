export module ISA:Instruction;

import std;

import :InstructionData;
import :Opcodes;
import :Registers;

namespace ISA {

export {
    struct Instruction {
        Opcodes::Opcode opcode;
        InstructionData data;

        constexpr Instruction(uint32_t bits);

        constexpr explicit operator uint32_t() const {
            return data.visit([](auto&& x) {
                return std::bit_cast<uint32_t>(x);
            });
        }
    };
}

// implementation

namespace Impl {
// Returns the fully-resolved opcode for an instruction
constexpr auto opcodeFor(uint32_t bits) -> Opcodes::Opcode {
    using namespace Opcodes;
    const auto opcode = Util::scopedEnumCast<OPCODE>(bits >> 26);
    const auto instR  = std::bit_cast<CPU::TypeR>(bits);

    switch (opcode) {
        case OPCODE::OP_SPECIAL: {
            return Util::scopedEnumCast<SPECIAL>(instR.func);
        }
        case OPCODE::OP_REGIMM: {
            return Util::scopedEnumCast<REGIMM_rt>(instR.rt);
        }
        case OPCODE::OP_COP0: {
            if ((0xFE000000 & bits) >> 6 == 0) {
                return Util::scopedEnumCast<CP0>(instR.func);
            }
        }
            [[fallthrough]];
        case OPCODE::OP_COP1:
            [[fallthrough]];
        case OPCODE::OP_COP2: {
            const auto rsOpcode = Util::scopedEnumCast<COPz_rs>(instR.rs);
            if (rsOpcode == COPz_rs::OP_BC) {
                return Util::scopedEnumCast<COPz_rt>(instR.rt);
            } else {
                return rsOpcode;
            }
        }
        default:
            return opcode;
    }
}

template <typename E>
constexpr auto getInstructionData(E opcode, uint32_t bits) -> InstructionData {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^E)) {
        if (opcode == std::meta::extract<E>(e)) {
            if constexpr (constexpr auto anns = Util::staticAnnotationsOf(e); !anns.empty()) {
                constexpr auto operandType = Util::dealiasedTypeOf(anns.front());
                return std::bit_cast<typename[:operandType:] ::InstType>(bits);
            }
        }
    }
    return bits;
}

template <typename E>
constexpr auto formatOps(E opcode, InstructionData data) -> std::vector<std::string> {
    auto result = std::vector<std::string>{};

    template for (constexpr auto e : Util::staticEnumeratorsOf(^^E)) {
        if (opcode == std::meta::extract<E>(e)) {
            if constexpr (constexpr auto anns = Util::staticAnnotationsOf(e); !anns.empty()) {
                constexpr auto operandType = Util::dealiasedTypeOf(anns.front());

                constexpr static auto args = std::define_static_array(
                    std::meta::template_arguments_of(operandType) | std::views::drop(1));

                template for (constexpr auto a : args) {
                    auto instData = std::get<typename[:operandType:] ::InstType>(data);
                    // gcc bug prevents usage of instData.[:a:]
                    // use this as workaround for now
                    constexpr auto name = std::meta::identifier_of([:a:]);
                    if constexpr (name == "rt") {
                        result.push_back(std::format("{}", static_cast<ISA::CPU_REG>(instData.rt)));
                    } else if constexpr (name == "rs") {
                        result.push_back(std::format("{}", static_cast<ISA::CPU_REG>(instData.rs)));
                    } else if constexpr (name == "imm") {
                        result.push_back(std::format("{:#x}", (int16_t)instData.imm));
                    } else if constexpr (name == "tgt") {
                        // shift left 2 and append high order 4 bits (always 0x8?)
                        result.push_back(std::format("0x8{:05x}", (uint32_t)instData.tgt << 2));
                    } else if constexpr (name == "rd") {
                        result.push_back(std::format("{}", static_cast<ISA::CPU_REG>(instData.rd)));
                    } else if constexpr (name == "sa") {
                        result.push_back(std::format("{}", (uint32_t)instData.sa));
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
    if (std::holds_alternative<Opcodes::SPECIAL>(inst.opcode) &&
        std::get<Opcodes::SPECIAL>(inst.opcode) == Opcodes::SPECIAL::OP_SLL &&
        std::get<CPU::TypeR>(inst.data).sa == 0) {
        return std::format("{} ", "NOP");
    }

    auto instStr = std::string{};

    // print the opcode
    auto opcode = inst.opcode.visit([](auto&& opcode) {
        return Util::enumName(opcode);
    });
    if (opcode.has_value()) {
        // insert coprocessor number
        if (auto it = opcode->find("Cz"); it != opcode->npos) {
            auto cpIndex = (static_cast<uint32_t>(inst) >> 25) & 0b11;
            opcode->replace(it, 2, std::format("C{}", cpIndex));
        }
        instStr += std::format("{:8}", std::string_view(*opcode).substr(3));
    } else {
        inst.data.visit([&](auto&& operand) {
            instStr += std::format("UNKNOWN INSTRUCTION (0x{:08X})", std::bit_cast<uint32_t>(operand));
        });
    }

    // print the operands
    inst.opcode.visit([&](auto&& opcode) {
        auto ops = Impl::formatOps(opcode, inst.data);
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
    });

    return instStr;
}
} // namespace Impl

constexpr Instruction::Instruction(uint32_t bits) {
    opcode = Impl::opcodeFor(bits);
    data   = opcode.visit([=](auto&& opcode) {
        return Impl::getInstructionData(opcode, bits);
    });
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
