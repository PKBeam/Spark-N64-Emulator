#include <filesystem>
#include <stdexcept>
#if defined(__linux__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif
#include "file.hpp"

namespace Util {

#if defined(__linux__)
auto memMapFile(std::filesystem::path path) -> std::byte* {
    auto filePath = std::filesystem::path{path};
    auto fileSize = std::filesystem::file_size(filePath);
    auto fd       = open(path.c_str(), O_RDONLY);
    auto ptr      = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (ptr == MAP_FAILED) {
        throw std::runtime_error("Failed to map file from memory");
    }
    return static_cast<std::byte*>(ptr);
}

auto memUnmapFile(std::filesystem::path path, std::byte* ptr) -> void {
    auto fileSize = std::filesystem::file_size(path);
    if (munmap(ptr, fileSize) != 0) {
        throw std::runtime_error("Failed to unmap file from memory");
    }
}
#elif defined(_WIN32)
auto memMapFile(std::filesystem::path path) -> std::byte* {
    auto hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open file for memory mapping");
    }
    auto hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (hMap == nullptr) {
        CloseHandle(hFile);
        throw std::runtime_error("Failed to create file mapping for memory mapping");
    }
    auto ptr = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (ptr == nullptr) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        throw std::runtime_error("Failed to map view of file for memory mapping");
    }
    CloseHandle(hMap);
    CloseHandle(hFile);
    return static_cast<std::byte*>(ptr);
}

auto memUnmapFile(std::filesystem::path path, std::byte* ptr) -> void {
    if (UnmapViewOfFile(ptr) == 0) {
        throw std::runtime_error("Failed to unmap view of file for memory mapping");
    }
}
#endif

} // namespace Util
