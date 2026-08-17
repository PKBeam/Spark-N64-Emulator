module;

import std;

export module Util:Bit;

import :Types;

export namespace Util {

template <typename T>
constexpr auto byteswapMembers(T* obj) -> void {
    constexpr static auto members = std::define_static_array(
        std::meta::members_of(^^T, std::meta::access_context::unchecked()));
    template for (constexpr auto member : members) {
        if constexpr (std::meta::is_nonstatic_data_member(member)) {
            using MemberT = std::remove_cvref_t<decltype(obj->[:member:])>;
            if constexpr (std::is_integral_v<MemberT> && !std::meta::is_bit_field(member)) {
                obj->[:member:] = std::byteswap(obj->[:member:]);
            } else if constexpr (std::is_class_v<MemberT>) {
                byteswapMembers(&obj->[:member:]);
            }
        }
    }
}

constexpr auto isBigEndian() -> bool {
    return std::endian::native == std::endian::big;
}

constexpr auto isLittleEndian() -> bool {
    return std::endian::native == std::endian::little;
}

template <std::integral T>
constexpr auto byteswapIfLittleEndian(T& data) -> void {
    if constexpr (std::endian::native == std::endian::little) {
        data = std::byteswap(data);
    }
}

template <std::integral T>
constexpr auto bytesFromKiB(T KiB) -> T {
    return 1024 * KiB;
}

template <std::integral T>
constexpr auto bytesFromMiB(T MiB) -> T {
    return 1024 * 1024 * MiB;
}

template <std::signed_integral To, std::integral From>
constexpr auto signExt(From bits) -> To {
    if constexpr (std::is_same_v<bool, From>) {
        return static_cast<std::make_signed_t<To>>(bits);
    } else {
        auto val = static_cast<std::make_signed_t<From>>(bits);
        return static_cast<std::make_signed_t<To>>(val);
    }
}

template <std::integral From>
constexpr auto signExt32(From bits) -> int32_t {
    return signExt<int32_t>(bits);
}

template <std::integral From>
constexpr auto signExt64(From bits) -> int64_t {
    return signExt<int64_t>(bits);
}

} // namespace Util