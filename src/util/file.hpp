#pragma once
#include <cstddef>

namespace Util {

// Linux and Windows have different character sets for filenames
#if defined(__linux__) || defined(__APPLE__)
using OsChar_t = char;
#elif defined(_WIN32)
using OsChar_t = wchar_t;
#endif

auto memMapFile(const OsChar_t* path) -> std::byte*;
auto memUnmapFile(const OsChar_t* path, std::byte* ptr) -> void;

} // namespace Util
