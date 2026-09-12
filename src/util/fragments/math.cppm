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

template <std::floating_point T>
constexpr auto isPosInf(T value) -> bool {
    return std::isinf(value) && value > 0;
}

template <std::floating_point T>
constexpr auto isNegInf(T value) -> bool {
    return std::isinf(value) && value < 0;
}

template <std::integral T, std::integral U>
constexpr auto clamp(U value_) -> T {
    const auto     value = static_cast<int64_t>(value_);
    constexpr auto min   = static_cast<int64_t>(std::numeric_limits<T>::min());
    constexpr auto max   = static_cast<int64_t>(std::numeric_limits<T>::max());
    return std::clamp(value, min, max);
}

template <std::integral T>
constexpr auto sign(T value) -> int {
    if (value > 0) {
        return 1;
    }
    if (value < 0) {
        return -1;
    }
    return 0;
}

template <bool Signed, std::size_t IntBits, std::size_t FracBits, std::integral T, std::floating_point U = float>
    requires(FracBits < 8 * sizeof(T) && std::is_unsigned_v<T>)
constexpr auto toFloat(T fixedPoint) -> U {
    constexpr auto scale = static_cast<U>(static_cast<T>(1) << FracBits);
    if constexpr (Signed) { // sign extension
        if ((fixedPoint >> (IntBits + FracBits - 1)) & 1) {
            fixedPoint |= ~((static_cast<T>(1) << (IntBits + FracBits)) - 1);
        }
        return static_cast<std::make_signed_t<T>>(fixedPoint) / scale;
    }
    return fixedPoint / scale;
}

template <bool Signed, std::size_t IntBits, std::size_t FracBits, std::integral T>
    requires(FracBits < 8 * sizeof(T))
constexpr auto toFixedS15_16(T fixedPoint) -> int32_t {
    if constexpr (FracBits < 16) {
        fixedPoint = static_cast<T>(fixedPoint) << (16 - FracBits);
    } else {
        fixedPoint = static_cast<T>(fixedPoint) >> (FracBits - 16);
    }
    if constexpr (Signed) { // sign extension
        if ((fixedPoint >> (IntBits + FracBits - 1)) & 1) {
            fixedPoint |= ~((static_cast<T>(1) << (IntBits + FracBits)) - 1);
        }
    }
    return fixedPoint;
}

struct Colour {
    float r;
    float g;
    float b;
    float a;
};

template <typename T = float>
    requires(std::is_arithmetic_v<T>)
struct Point {
    T x;
    T y;
    T z;
    constexpr Point(T a, T b, T c = 0) : x(a), y(b), z(c) {}
};
static_assert(sizeof(Util::Point<int32_t>) == 3 * sizeof(int32_t));

template <typename T = float>
    requires(std::is_arithmetic_v<T>)
struct Triangle {
    Point<T> v0;
    Point<T> v1;
    Point<T> v2;
    constexpr Triangle(Point<T> a, Point<T> b, Point<T> c) : v0(a), v1(b), v2(c) {}
};
static_assert(sizeof(Util::Triangle<int32_t>) == 3 * sizeof(Util::Point<int32_t>));

template <typename T = float>
    requires(std::is_arithmetic_v<T>)
struct Rectangle {
    Point<T> v0; // upper left
    Point<T> v1; // lower right
    constexpr Rectangle(Point<T> a, Point<T> b) : v0(a), v1(b) {}
};
static_assert(sizeof(Util::Rectangle<int32_t>) == 2 * sizeof(Util::Point<int32_t>));

using RenderTriangle = std::array<int32_t, 9>; // v0x v0y v0z v1x v1y v1z v2x v2y v2z
} // namespace Util

template <>
struct std::formatter<Util::Colour> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const Util::Colour& c, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f}, {:.2f})", c.r, c.g, c.b, c.a);
    }
};

template <typename T>
struct std::formatter<Util::Point<T>> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const Util::Point<T>& point, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({:8.2f}, {:8.2f}, {:8.2f})", point.x, point.y, point.z);
    }
};

template <typename T>
struct std::formatter<Util::Triangle<T>> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const Util::Triangle<T>& triangle, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}, {}, {}", triangle.v0, triangle.v1, triangle.v2);
    }
};

template <typename T>
struct std::formatter<Util::Rectangle<T>> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const Util::Rectangle<T>& rectangle, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}, {}", rectangle.v0, rectangle.v1);
    }
};