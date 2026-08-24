export module ISA:Registers;

import std;
import Util;

using namespace std::string_view_literals;

export namespace ISA {

enum class CPU_REG : uint8_t {
    zero = 0,
    at   = 1,
    v0   = 2,
    v1   = 3,
    a0   = 4,
    a1   = 5,
    a2   = 6,
    a3   = 7,
    t0   = 8,
    t1   = 9,
    t2   = 10,
    t3   = 11,
    t4   = 12,
    t5   = 13,
    t6   = 14,
    t7   = 15,
    s0   = 16,
    s1   = 17,
    s2   = 18,
    s3   = 19,
    s4   = 20,
    s5   = 21,
    s6   = 22,
    s7   = 23,
    t8   = 24,
    t9   = 25,
    k0   = 26,
    k1   = 27,
    gp   = 28,
    sp   = 29,
    s8   = 30,
    ra   = 31,
};

enum class VEC_ELEM : uint8_t {
    // clang-format off
    // Lane selects
    NONE_0 [[=0,=1,=2,=3,=4,=5,=6,=7]] = 0,
    NONE_1 [[=0,=1,=2,=3,=4,=5,=6,=7]] = 1,
    e0q    [[=0,=0,=2,=2,=4,=4,=6,=6]] = 2,
    e1q    [[=1,=1,=3,=3,=5,=5,=7,=7]] = 3,
    e0h    [[=0,=0,=0,=0,=4,=4,=4,=4]] = 4,
    e1h    [[=1,=1,=1,=1,=5,=5,=5,=5]] = 5,
    e2h    [[=2,=2,=2,=2,=6,=6,=6,=6]] = 6,
    e3h    [[=3,=3,=3,=3,=7,=7,=7,=7]] = 7,
    e0     [[=1,=1,=1,=1,=1,=1,=1,=1]] = 8,
    e1     [[=1,=1,=1,=1,=1,=1,=1,=1]] = 9,
    e2     [[=2,=2,=2,=2,=2,=2,=2,=2]] = 10,
    e3     [[=3,=3,=3,=3,=3,=3,=3,=3]] = 11,
    e4     [[=4,=4,=4,=4,=4,=4,=4,=4]] = 12,
    e5     [[=5,=5,=5,=5,=5,=5,=5,=5]] = 13,
    e6     [[=6,=6,=6,=6,=6,=6,=6,=6]] = 14,
    e7     [[=7,=7,=7,=7,=7,=7,=7,=7]] = 15
    // clang-format on
};

struct CP0Status {
    uint32_t ie  : 1 = 0;
    uint32_t exl : 1 = 0;
    uint32_t erl : 1 = 1;
    uint32_t ksu : 2 = 0;
    uint32_t ux  : 1 = 0;
    uint32_t sx  : 1 = 0;
    uint32_t kx  : 1 = 0;
    uint32_t im  : 8 = 0;
    // Diagnostic Status bits
    uint32_t de  : 1 = 0;
    uint32_t ce  : 1 = 0;
    uint32_t ch  : 1 = 0;
    uint32_t     : 1;
    uint32_t sr  : 1 = 0; // TODO set 1 on soft reset/NMI interrupt
    uint32_t ts  : 1 = 0;
    uint32_t bev : 1 = 1;
    uint32_t     : 1;
    uint32_t its : 1 = 0;
    //
    uint32_t re : 1 = 0;
    uint32_t fr : 1 = 0;
    uint32_t rp : 1 = 0;
    uint32_t cu : 4 = 0;
};

struct CP0Cause {
    uint32_t     : 2;
    uint32_t exc : 5 = 0;
    uint32_t     : 1;
    uint32_t ip  : 8 = 0;
    uint32_t     : 12;
    uint32_t ce  : 2 = 0;
    uint32_t     : 1;
    uint32_t bd  : 1 = 0;
};

enum class CP0_EXCEPTION_CODE : uint8_t {
    INTERRUPT     = 0,
    TLB_MOD       = 1,
    TLB_LOAD      = 2,
    TLB_STORE     = 3,
    ADDR_LOAD     = 4,
    ADDR_STORE    = 5,
    INST_BUS_ERR  = 6,
    DATA_BUS_ERR  = 7,
    SYSCALL       = 8,
    BREAKPOINT    = 9,
    RESERVED_INST = 10,
    COP_UNUSABLE  = 11,
    OVERFLOW      = 12,
    TRAP          = 13,
    FLOAT         = 15,
    WATCH         = 23,
};

enum class CP0_REG : uint8_t {
    INDEX                   = 0,
    RANDOM                  = 1,
    ENTRYLO0                = 2,
    ENTRYLO1                = 3,
    CONTEXT                 = 4,
    PAGEMASK                = 5,
    WIRED                   = 6,
    BADVADDR                = 8,
    COUNT                   = 9,
    ENTRYHI                 = 10,
    COMPARE                 = 11,
    STATUS[[= ^^CP0Status]] = 12,
    CAUSE[[= ^^CP0Cause]]   = 13,
    EPC                     = 14,
    PRID                    = 15,
    CONFIG                  = 16,
    LLADDR                  = 17,
    WATCHLO                 = 18,
    WATCHHI                 = 19,
    XCONTEXT                = 20,
    PARITYERR               = 26,
    CACHEERR                = 27,
    TAGLO                   = 28,
    TAGHI                   = 29,
    ERROREPC                = 30,
};

} // namespace ISA

template <>
struct std::formatter<ISA::CPU_REG> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const ISA::CPU_REG& reg, std::format_context& ctx) const {
        auto str = Util::enumName(reg).value_or(std::format("r{}", static_cast<uint8_t>(reg)));
        return std::format_to(ctx.out(), "{}", str);
    }
};

template <>
struct std::formatter<ISA::VEC_ELEM> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const ISA::VEC_ELEM& elem, std::format_context& ctx) const {
        if (elem == ISA::VEC_ELEM::NONE_0 || elem == ISA::VEC_ELEM::NONE_1) {
            return std::format_to(ctx.out(), "");
        }
        if (auto str = Util::enumName(elem)) {
            return std::format_to(ctx.out(), "e({})", str->substr(1)); // remove the leading 'e' from the enum name
        }
        return std::format_to(ctx.out(), "elem:{}", static_cast<uint8_t>(elem));
    }
};

template <>
struct std::formatter<ISA::CP0_REG> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const ISA::CP0_REG& reg, std::format_context& ctx) const {
        auto str = Util::enumName(reg).value_or(std::format("{}", static_cast<uint8_t>(reg)));
        return std::format_to(ctx.out(), "{}", str);
    }
};
