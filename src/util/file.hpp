#pragma once
#include <cstddef>

namespace Util {

// Linux and Windows have different character sets for filenames

#if defined(__linux__)
auto memMapFile(const char* path) -> std::byte*;
auto memUnmapFile(const char* path, std::byte* ptr) -> void;
#elif defined(_WIN32)
auto memMapFile(const wchar_t* path) -> std::byte*;
auto memUnmapFile(const wchar_t* path, std::byte* ptr) -> void;
#endif

} // namespace Util
