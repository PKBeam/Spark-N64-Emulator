#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>
using namespace std::string_view_literals;

namespace Util {
namespace VK {

inline auto readSpirvShader(std::filesystem::path path) -> std::vector<uint32_t> {
    auto file = std::ifstream(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open SPIR-V shader file!");
    }
    const auto size   = std::filesystem::file_size(path);
    auto       buffer = std::vector<uint32_t>(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

inline auto shaderModuleCreateInfo(const std::vector<uint32_t>& shaderCode) -> VkShaderModuleCreateInfo {
    return VkShaderModuleCreateInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .codeSize = shaderCode.size() * sizeof(uint32_t),
        .pCode    = shaderCode.data(),
    };
}

} // namespace VK
} // namespace Util
