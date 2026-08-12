export module ISA:Registers;

import std;
import Util;

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

enum class CP0_REG : uint8_t {
    INDEX     = 0,
    RANDOM    = 1,
    ENTRYLO0  = 2,
    ENTRYLO1  = 3,
    CONTEXT   = 4,
    PAGEMASK  = 5,
    WIRED     = 6,
    BADVADDR  = 8,
    COUNT     = 9,
    ENTRYHI   = 10,
    COMPARE   = 11,
    STATUS    = 12,
    CAUSE     = 13,
    EPC       = 14,
    PRID      = 15,
    CONFIG    = 16,
    LLADDR    = 17,
    WATCHLO   = 18,
    WATCHHI   = 19,
    XCONTEXT  = 20,
    PARITYERR = 26,
    CACHEERR  = 27,
    TAGLO     = 28,
    TAGHI     = 29,
    ERROREPC  = 30,
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
struct std::formatter<ISA::CP0_REG> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const ISA::CP0_REG& reg, std::format_context& ctx) const {
        auto str = Util::enumName(reg).value_or(std::format("{}", static_cast<uint8_t>(reg)));
        return std::format_to(ctx.out(), "{}", str);
    }
};