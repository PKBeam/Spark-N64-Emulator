#pragma once

#ifdef _MSC_VER
#define STRUCT_PACKED(name) \
    __pragma(pack(push, 1)) struct name __pragma(pack(pop))
#elif defined(__GNUC__)
#define STRUCT_PACKED(name) struct __attribute__((packed)) name
#endif

#if defined(__clang__) && !defined(__cpp_lib_reflection)
namespace std {
namespace meta {
struct info {};
} // namespace meta
} // namespace std
#endif

#if !defined(__cpp_contracts)
#define pre(x)
#define post(x)
#define contract_assert(x)
#endif

#define IF_LOG_ENABLED(logger) if (logger && logger->enabled())

#define WITH_LOG_DISABLED(logger, expr)         \
    [&]<typename Result = decltype(expr)>() {   \
        bool was_enabled = false;               \
        IF_LOG_ENABLED(logger) {                \
            was_enabled = true;                 \
            logger->disable();                  \
        }                                       \
        if constexpr (std::is_void_v<Result>) { \
            expr;                               \
            if (was_enabled) {                  \
                logger->enable();               \
            }                                   \
        } else {                                \
            auto result = expr;                 \
            if (was_enabled) {                  \
                logger->enable();               \
            }                                   \
            return result;                      \
        }                                       \
    }();

#define HEXFMT8 "{:#04x}"
#define HEXFMT12 "{:#05x}"
#define HEXFMT16 "{:#06x}"
#define HEXFMT24 "{:#08x}"
#define HEXFMT32 "{:#010x}"
#define HEXFMT48 "{:#014x}"
#define HEXFMT64 "{:#018x}"

#define STD_FORMATTER_ENUM(ENUM, FMT_FUNC)                                         \
    template <>                                                                    \
    struct std::formatter<ENUM> {                                                  \
        constexpr auto parse(std::format_parse_context& ctx)                       \
            -> std::format_parse_context::iterator {                               \
            return ctx.begin();                                                    \
        }                                                                          \
                                                                                   \
        constexpr auto format(const ENUM& value, std::format_context& ctx) const { \
            return std::format_to(ctx.out(), "{}", FMT_FUNC(value));               \
        }                                                                          \
    }

#define STD_FORMATTER_ENUM_NAME(ENUM)                                   \
    STD_FORMATTER_ENUM(ENUM, [](auto&& e) {                             \
        return Util::enumName(e).value_or(                              \
            std::format("{}({})",                                       \
                        std::meta::display_string_of(^^ENUM),           \
                        static_cast<std::underlying_type_t<ENUM>>(e))); \
    })
