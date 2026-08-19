module;

#if defined(__linux__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

export module Util:File;
import :Types;

import std;

export namespace Util {

#if defined(__linux__)
auto memMapFile(std::filesystem::path path) -> std::byte* {
    auto filePath = std::filesystem::path{path};
    auto fileSize = std::filesystem::file_size(filePath);
    auto fd       = open(path.c_str(), O_RDONLY);
    auto ptr      = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (ptr == MAP_FAILED) {
        throw Util::Error("Failed to map file from memory");
    }
    return static_cast<std::byte*>(ptr);
}

auto memUnmapFile(std::filesystem::path path, std::byte* ptr) -> void {
    auto fileSize = std::filesystem::file_size(path);
    if (munmap(ptr, fileSize) != 0) {
        throw Util::Error("Failed to unmap file from memory");
    }
}
#endif

} // namespace Util
