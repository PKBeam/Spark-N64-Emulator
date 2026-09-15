module;
#include <cfenv>
#include "defines.hpp"
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
        return (std::bit_cast<uint32_t>(value) & 0x00400000) != 0;
    } else if constexpr (sizeof(T) == 8) {
        return (std::bit_cast<uint64_t>(value) & 0x0008000000000000) != 0;
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

template <typename T>
struct Point {
    T x;
    T y;
    T z;
    constexpr Point(T a, T b, T c = T()) : x(a), y(b), z(c) {}
    template <typename U>
        requires(std::is_convertible_v<U, T>)
    constexpr Point(const Point<U>& other) {
        x = static_cast<T>(other.x);
        y = static_cast<T>(other.y);
        z = static_cast<T>(other.z);
    }
    constexpr auto operator*(T k) -> Point<T> {
        return Point(x * k, y * k, z * k);
    }
    constexpr auto operator/(T k) -> Point<T> {
        return Point(x / k, y / k, z / k);
    }
};
static_assert(sizeof(Util::Point<int32_t>) == 3 * sizeof(int32_t));

template <typename T>
    requires(std::is_default_constructible_v<T>)
struct Triangle {
    std::array<T, 9> vertices{};
    constexpr Triangle() : vertices{} {
    }
    constexpr Triangle(Point<T> a, Point<T> b, Point<T> c) {
        vertices[0] = a.x;
        vertices[1] = a.y;
        vertices[2] = a.z;
        vertices[3] = b.x;
        vertices[4] = b.y;
        vertices[5] = b.z;
        vertices[6] = c.x;
        vertices[7] = c.y;
        vertices[8] = c.z;
    }
    template <typename U>
        requires(std::is_convertible_v<U, T>)
    constexpr Triangle(const Triangle<U>& other) {
        for (std::size_t i = 0; i < 9; ++i) {
            vertices[i] = static_cast<T>(other.vertices[i]);
        }
    }
    constexpr auto v0() const -> Point<T> {
        return Point<T>(vertices[0], vertices[1], vertices[2]);
    }
    constexpr auto v1() const -> Point<T> {
        return Point<T>(vertices[3], vertices[4], vertices[5]);
    }
    constexpr auto v2() const -> Point<T> {
        return Point<T>(vertices[6], vertices[7], vertices[8]);
    }
    template <typename Self>
    constexpr auto data(this Self&& self) {
        return self.vertices.data();
    }
    template <typename Self>
    constexpr auto bytes(this Self&& self) {
        return reinterpret_cast<const std::byte*>(self.vertices.data());
    }
    constexpr auto operator*=(T scalar) -> Triangle<T>& {
        for (std::size_t i = 0; i < 9; ++i) {
            vertices[i] *= scalar;
        }
        return *this;
    }
    constexpr auto operator/=(T scalar) -> Triangle<T>& {
        for (std::size_t i = 0; i < 9; ++i) {
            vertices[i] /= scalar;
        }
        return *this;
    }
};
static_assert(sizeof(Util::Triangle<int32_t>) == 3 * sizeof(Util::Point<int32_t>));

template <typename T>
struct Rectangle {
    Point<T> v0; // upper left
    Point<T> v1; // lower right
    constexpr Rectangle(Point<T> a, Point<T> b) : v0(a), v1(b) {}
    constexpr auto triangles() const -> std::pair<Triangle<T>, Triangle<T>> {
        return {
            Triangle<T>(v0, Point<T>(v0.x, v1.y), v1),
            Triangle<T>(v0, Point<T>(v1.x, v0.y), v1),
        };
    }
};
static_assert(sizeof(Util::Rectangle<int32_t>) == 2 * sizeof(Util::Point<int32_t>));

struct FixedPointZero {};
constexpr auto Fxp_0 = FixedPointZero{};

template <bool Signed, std::size_t IntBits, std::size_t FracBits>
    requires(IntBits > 0 && IntBits + FracBits <= 64)
struct FixedPoint {
    static consteval auto valueType() {
        auto type = (IntBits + FracBits <= 32) ? ^^uint32_t : ^^uint64_t;
        return Signed ? std::meta::make_signed(type) : type;
    }
    using Type = typename[:valueType():];

    static constexpr auto scale     = Type(1) << FracBits;
    static constexpr auto valueMask = [] {
        if constexpr (IntBits + FracBits >= 8 * sizeof(Type)) {
            return static_cast<Type>(-1);
        } else {
            return (Type(1) << (IntBits + FracBits)) - 1;
        }
    }();
    static constexpr auto fractionMask = scale - 1;

    Type value;

    constexpr FixedPoint() : value(0) {}
    constexpr FixedPoint(FixedPointZero) : value(0) {}

    template <std::floating_point F>
    constexpr FixedPoint(F f) : value(static_cast<Type>(f * scale)) {}

    constexpr FixedPoint(Type) = delete ("Raw integer input is ambiguous; use fromBits() or fromValue() instead");

    constexpr FixedPoint(Type integer, uint32_t frac) {
        *this = fromBits((integer << FracBits) | (frac & fractionMask));
    }

    static constexpr auto fromBits(Type v) -> FixedPoint {
        auto result  = FixedPoint();
        result.value = static_cast<Type>(v & valueMask);
        if constexpr (Signed) {
            constexpr auto emptyUpperBits = 8 * sizeof(Type) - (IntBits + FracBits);
            result.value                  = (result.value << emptyUpperBits) >> emptyUpperBits;
        }
        return result;
    }

    static constexpr auto fromValue(Type v) -> FixedPoint {
        return FixedPoint(v, 0);
    }

    template <bool OtherSigned, std::size_t OtherIntBits, std::size_t OtherFracBits>
    constexpr FixedPoint(const FixedPoint<OtherSigned, OtherIntBits, OtherFracBits>& other) {
        auto otherValue = other.value;
        if constexpr (OtherFracBits > FracBits) {
            value = other.value >> (OtherFracBits - FracBits);
        } else {
            value = static_cast<Type>(otherValue) << (FracBits - OtherFracBits);
        }
    }

    constexpr auto integer() const -> Type {
        return value >> FracBits;
    }

    constexpr auto fraction() const -> Type {
        return value & fractionMask;
    }

    constexpr auto bits() const -> Type {
        return value;
    }

    constexpr auto floor() const -> FixedPoint {
        return fromBits((value >> FracBits) << FracBits);
    }

    template <std::floating_point F>
    constexpr operator F() const {
        return static_cast<F>(value) / static_cast<F>(scale);
    }

    constexpr auto operator+(const FixedPoint& other) const {
        return fromBits(static_cast<Type>(value) + static_cast<Type>(other.value));
    }
    constexpr auto operator-(const FixedPoint& other) const {
        return fromBits(static_cast<Type>(value) - static_cast<Type>(other.value));
    }

    template <bool OtherSigned, std::size_t OtherIntBits, std::size_t OtherFracBits>
        requires(std::max(IntBits, OtherIntBits) + std::max(FracBits, OtherFracBits) <= 32 &&
                 (Signed != OtherSigned || IntBits != OtherIntBits || FracBits != OtherFracBits))
    constexpr auto operator+(const FixedPoint<OtherSigned, OtherIntBits, OtherFracBits>& other) const {
        using OutSigned   = std::bool_constant<Signed || OtherSigned>;
        using OutIntBits  = std::integral_constant<std::size_t, std::max(IntBits, OtherIntBits)>;
        using OutFracBits = std::integral_constant<std::size_t, std::max(FracBits, OtherFracBits)>;
        using OutType     = FixedPoint<OutSigned::value, OutIntBits::value, OutFracBits::value>;
        return FixedPoint(OutType(*this) + OutType(other));
    }

    template <bool OtherSigned, std::size_t OtherIntBits, std::size_t OtherFracBits>
        requires(std::max(IntBits, OtherIntBits) + std::max(FracBits, OtherFracBits) <= 32 &&
                 (Signed != OtherSigned || IntBits != OtherIntBits || FracBits != OtherFracBits))
    constexpr auto operator-(const FixedPoint<OtherSigned, OtherIntBits, OtherFracBits>& other) const {
        using OutSigned   = std::bool_constant<Signed || OtherSigned>;
        using OutIntBits  = std::integral_constant<std::size_t, std::max(IntBits, OtherIntBits)>;
        using OutFracBits = std::integral_constant<std::size_t, std::max(FracBits, OtherFracBits)>;
        using OutType     = FixedPoint<OutSigned::value, OutIntBits::value, OutFracBits::value>;
        return FixedPoint(OutType(*this) - OutType(other));
    }

    template <bool OtherSigned, std::size_t OtherIntBits, std::size_t OtherFracBits>
        requires(IntBits + OtherIntBits + FracBits + OtherFracBits <= 64) // overflow
    constexpr auto operator*(const FixedPoint<OtherSigned, OtherIntBits, OtherFracBits>& other) const {
        using OutSigned   = std::bool_constant<Signed || OtherSigned>;
        using OutFracBits = std::integral_constant<std::size_t, FracBits + OtherFracBits>;
        using OutIntBits  = std::integral_constant<std::size_t, IntBits + OtherIntBits>;
        using OutType     = FixedPoint<OutSigned::value, OutIntBits::value, OutFracBits::value>;
        auto result       = [this, &other] {
            if constexpr (Signed || OtherSigned) {
                return static_cast<int64_t>(value) * static_cast<int64_t>(other.value);
            } else {
                return static_cast<uint64_t>(value) * static_cast<uint64_t>(other.value);
            }
        }();
        return OutType::fromBits(result);
    }
};

template <std::size_t IntBits, std::size_t FracBits>
using UFixedPoint = FixedPoint<false, IntBits, FracBits>;

template <std::size_t IntBits, std::size_t FracBits>
using SFixedPoint = FixedPoint<true, IntBits, FracBits>;

// [xyzxyzxyz] where x,y = s16.16
using RenderTriangle = Util::Triangle<SFixedPoint<16, 16>>;

} // namespace Util

template <typename T>
struct std::formatter<Util::Point<T>> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const Util::Point<T>& point, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "(" SPIXFMT ", " SPIXFMT ", " SPIXFMT ")", static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
};

template <typename T>
struct std::formatter<Util::Triangle<T>> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const Util::Triangle<T>& triangle, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}, {}, {}", triangle.v0(), triangle.v1(), triangle.v2());
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