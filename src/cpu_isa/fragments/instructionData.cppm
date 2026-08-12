export module ISA:InstructionData;

import std;
import Util;

export namespace ISA {

namespace CPU {

struct TypeI {
    uint32_t imm : 16;
    uint32_t rt  : 5;
    uint32_t rs  : 5;
    uint32_t op  : 6;
};

struct TypeJ {
    uint32_t tgt : 26;
    uint32_t op  : 6;
};

struct TypeR {
    uint32_t func : 6;
    uint32_t sa   : 5;
    uint32_t rd   : 5;
    uint32_t rt   : 5;
    uint32_t rs   : 5;
    uint32_t op   : 6;
};
} // namespace CPU

namespace FPU {
struct TypeI {
    uint32_t off  : 16;
    uint32_t ft   : 5;
    uint32_t base : 5;
    uint32_t op   : 6;
};

struct TypeOther {
    uint32_t zero_ : 11;
    uint32_t fs    : 5;
    uint32_t rt    : 5;
    uint32_t sub   : 5;
    uint32_t op    : 6;
};

struct TypeR {
    uint32_t func : 6;
    uint32_t fd   : 5;
    uint32_t fs   : 5;
    uint32_t ft   : 5;
    uint32_t fmt  : 5;
    uint32_t op   : 6;
};
} // namespace FPU

using InstructionData = std::variant<uint32_t, CPU::TypeI, CPU::TypeJ, CPU::TypeR>;

} // namespace ISA