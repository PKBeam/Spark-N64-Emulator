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
