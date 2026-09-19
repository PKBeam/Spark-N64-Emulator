#pragma once
#include <filesystem>
#include <cstddef>

namespace Util {

auto memMapFile(std::filesystem::path path) -> std::byte*;
auto memUnmapFile(std::filesystem::path path, std::byte* ptr) -> void;

} // namespace Util
