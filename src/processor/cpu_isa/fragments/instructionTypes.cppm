export module ISA:InstructionTypes;

import std;
import Util;
import :Registers;

template <std::ranges::input_range R>
consteval auto FmtStr(R&& s) {
    return Util::staticString(s);
}

constexpr auto FmtNone = FmtStr("{}");

export namespace ISA {

template <std::meta::info FormatString = FmtNone, typename T = uint32_t>
struct OpFormat {};

// clang-format off
namespace CPU {
struct TypeI {
    uint32_t imm [[=^^OpFormat<FmtStr("{:#x}")>]]  : 16;
    uint32_t rt  [[=^^OpFormat<FmtNone, CPU_REG>]] : 5;
    uint32_t rs  [[=^^OpFormat<FmtNone, CPU_REG>]] : 5;
    uint32_t op                                    : 6;
};

struct TypeJ {
    uint32_t tgt [[=^^OpFormat<FmtStr("{:#x}")>]] : 26;
    uint32_t op                                   : 6;
};

struct TypeR {
    uint32_t func                                   : 6;
    uint32_t sa   [[=^^OpFormat<>]]                 : 5;
    uint32_t rd   [[=^^OpFormat<FmtNone, CPU_REG>]] : 5;
    uint32_t rt   [[=^^OpFormat<FmtNone, CPU_REG>]] : 5;
    uint32_t rs   [[=^^OpFormat<FmtNone, CPU_REG>]] : 5;
    uint32_t op                                     : 6;
};
} // namespace CPU


namespace FPU {
struct TypeI {
    uint32_t imm [[=^^OpFormat<FmtStr("{:#x}")>]]  : 16;
    uint32_t ft  [[=^^OpFormat<FmtStr("$f{}")>]]   : 5;
    uint32_t rs  [[=^^OpFormat<FmtNone, CPU_REG>]] : 5;
    uint32_t op                                    : 6;
};

struct TypeR {
    uint32_t func                                      : 6;
    uint32_t fd   [[=^^OpFormat<FmtStr("$f{}")>]]      : 5;
    uint32_t fs   [[=^^OpFormat<FmtStr("$f{}")>]]      : 5;
    uint32_t ft   [[=^^OpFormat<FmtStr("$f{}")>]]      : 5;
    uint32_t fmt  [[=^^OpFormat<FmtNone, CP1_FORMAT>]] : 5;
    uint32_t op                                        : 6;
};

struct TypeO {
    uint32_t                                        : 11;
    uint32_t fs   [[=^^OpFormat<FmtStr("$f{}")>]]   : 5;
    uint32_t rt   [[=^^OpFormat<FmtNone, CPU_REG>]] : 5;
    uint32_t subOp                                  : 5;
    uint32_t op                                     : 6;
};
} // namespace FPU

namespace RSP {
struct TypeVI {
    uint32_t imm    [[=^^OpFormat<FmtStr("{:#x}")>]]   : 7;
    uint32_t vtElem [[=^^OpFormat<FmtNone, VEC_ELEM>]] : 4;
    uint32_t func                                      : 5;
    uint32_t vt     [[=^^OpFormat<FmtStr("$v{}")>]]    : 5;
    uint32_t rs     [[=^^OpFormat<FmtNone, CPU_REG>]]  : 5;
    uint32_t op                                        : 6;
};

struct TypeVR {
    uint32_t func                                      : 6;
    uint32_t vd     [[=^^OpFormat<FmtStr("$v{}")>]]    : 5;
    uint32_t vs     [[=^^OpFormat<FmtStr("$v{}")>]]    : 5;
    uint32_t vt     [[=^^OpFormat<FmtStr("$v{}")>]]    : 5;
    uint32_t vtElem [[=^^OpFormat<FmtNone, VEC_ELEM>]] : 4;
    uint32_t _                                         : 1 = 1;
    uint32_t op                                        : 6;
};

struct TypeVS {
    uint32_t func                                      : 6;
    uint32_t vd     [[=^^OpFormat<FmtStr("$v{}")>]]    : 5;
    uint32_t vdElem [[=^^OpFormat<FmtNone, VEC_ELEM>]] : 5;
    uint32_t vt     [[=^^OpFormat<FmtStr("$v{}")>]]    : 5;
    uint32_t vtElem [[=^^OpFormat<FmtNone, VEC_ELEM>]] : 4;
    uint32_t _                                         : 1 = 1;
    uint32_t op                                        : 6;
};

struct TypeVM {
    uint32_t                                           : 7;
    uint32_t vsElem [[=^^OpFormat<FmtNone, VEC_ELEM>]] : 4;
    uint32_t vs     [[=^^OpFormat<FmtStr("$v{}")>]]    : 5;
    uint32_t rt     [[=^^OpFormat<FmtNone, CPU_REG>]]  : 5;
    uint32_t func                                      : 5;
    uint32_t op                                        : 6;
};
} // namespace RSP

} // namespace ISA