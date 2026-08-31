module;
#include <cfenv>
export module Util:Math;

import std;
import :Types;

export namespace Util {

enum class FP_ROUND_MODE : decltype(FE_DOWNWARD) {
    DOWN    = FE_DOWNWARD,
    NEAREST = FE_TONEAREST,
    TO_ZERO = FE_TOWARDZERO,
    UP      = FE_UPWARD,
};

template <typename Function>
constexpr auto withFpRoundMode(FP_ROUND_MODE roundMode, Function&& func) -> void {
    const auto prev = fegetround();
    fesetround(static_cast<decltype(FE_DOWNWARD)>(roundMode));
    func();
    fesetround(prev);
}

template <std::floating_point T>
constexpr auto isSNaN(T value) -> bool {
    if (!std::isnan(value)) {
        return false;
    }
    if constexpr (sizeof(T) == 4) {
        return (std::bit_cast<uint32_t>(value) & 0x00400000) == 0;
    } else if constexpr (sizeof(T) == 8) {
        return (std::bit_cast<uint64_t>(value) & 0x0008000000000000) == 0;
    } else {
        static_assert(false, "Unsupported floating point type");
    }
}

template <std::floating_point T>
constexpr auto isQNaN(T value) -> bool {
    if (!std::isnan(value)) {
        return false;
    }
    if constexpr (sizeof(T) == 4) {
        return (std::bit_cast<uint32_t>(value) & 0x00400000) == 1;
    } else if constexpr (sizeof(T) == 8) {
        return (std::bit_cast<uint64_t>(value) & 0x0008000000000000) == 1;
    } else {
        static_assert(false, "Unsupported floating point type");
    }
}

template <std::integral T, std::integral U>
constexpr auto clamp(U value_) -> T {
    const auto     value = static_cast<int64_t>(value_);
    constexpr auto min   = static_cast<int64_t>(std::numeric_limits<T>::min());
    constexpr auto max   = static_cast<int64_t>(std::numeric_limits<T>::max());
    return std::clamp(value, min, max);
}

} // namespace Util