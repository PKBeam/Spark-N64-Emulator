export module Util:Types;

import std;
import :Meta;

export using int8_t   = std::int8_t;
export using int16_t  = std::int16_t;
export using int32_t  = std::int32_t;
export using int64_t  = std::int64_t;
export using uint8_t  = std::uint8_t;
export using uint16_t = std::uint16_t;
export using uint32_t = std::uint32_t;
export using uint64_t = std::uint64_t;

export namespace Util {
struct Error : std::runtime_error {
    template <typename... Args>
    Error(std::format_string<Args...> fmt, Args&&... args)
        : std::runtime_error(std::format(fmt, std::forward<Args>(args)...)) {}
};

struct Range {
    uint32_t lower;
    uint32_t upper;

    constexpr auto size() const -> std::size_t {
        return upper - lower + 1;
    }
    constexpr auto contains(uint32_t addr) const -> bool {
        return lower <= addr && addr <= upper;
    }
};

template <typename E>
constexpr auto getRange(uint32_t rangeValue) -> std::pair<E, Util::Range> {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^E)) {
        constexpr auto ann   = std::meta::annotations_of(e).front();
        constexpr auto range = std::meta::extract<Util::Range>(ann);
        if (range.contains(rangeValue)) {
            return {[:e:], range};
        }
    }
    throw Util::Error("Unknown range value");
}

} // namespace Util