module;

import std;

export module Util:Meta;

#include "defines.hpp"

export namespace Util {

consteval auto isNull(std::meta::info i) {
    return i == std::meta::info{};
}

consteval auto staticEnumeratorsOf(std::meta::info i) {
    return std::define_static_array(std::meta::enumerators_of(i));
}

consteval auto staticAnnotationsOf(std::meta::info i) {
    return std::define_static_array(std::meta::annotations_of(i));
}

// extracts the dealiased reflected type `T`, from an annotation `[[=^^T]]`.
consteval auto dealiasedTypeOf(std::meta::info annotation) //
    pre(std::meta::is_annotation(annotation))              //
{
    return std::meta::dealias(std::meta::extract<std::meta::info>(annotation));
}

template <typename E>
constexpr auto enumName(E value) -> std::optional<std::string> {
    constexpr static auto enums = Util::staticEnumeratorsOf(^^E);
    template for (constexpr auto e : enums) {
        auto enumValue = std::meta::extract<E>(e);
        if (value == enumValue) {
            return std::string{std::meta::identifier_of(e)};
        }
    }
    return std::nullopt;
}

template <typename E, std::integral I>
constexpr auto scopedEnumCast(I value) -> E {
    return static_cast<E>(static_cast<std::underlying_type<E>::type>(value));
}

template <typename E, std::integral I>
constexpr auto enumIs(I rawValue, E enumValue) -> bool {
    return rawValue == static_cast<I>(enumValue);
}

} // namespace Util
