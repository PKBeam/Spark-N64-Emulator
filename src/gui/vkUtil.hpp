#pragma once

#include <QVulkanWindow>
#include <QVulkanFunctions>

#include <string_view>
#include <format>
using namespace std::string_view_literals;

namespace GUI {
namespace VK {
inline auto tryFunc(decltype(VK_SUCCESS) expr, std::string_view desc = ""sv) {
    if (expr != VK_SUCCESS) {
        qFatal("Failed to %s: %d", desc.data(), static_cast<int>(expr));
    }
}

template <typename T>
inline auto aligned(T v, T byteAlign) -> T {
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

} // namespace VK
} // namespace GUI
