#pragma once
#include <cstddef>

namespace Util {

auto memMapFile(const char* path) -> std::byte*;
auto memUnmapFile(const char* path, std::byte* ptr) -> void;

} // namespace Util
