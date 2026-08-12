export module Rom:File;

import std;
import Util;

import :Types;

export class RomFile {
  public:
    constexpr RomFile(std::filesystem::path path);
    constexpr ~RomFile();

    constexpr auto readHeader() const
        -> N64RomHeader;

    template <std::integral T>
    constexpr auto read(uint64_t addr) const
        -> T;

    constexpr auto size() const
        -> std::size_t;

    constexpr auto data() const
        -> std::byte*;

  private:
    std::filesystem::path m_romFilePath;
    std::size_t           m_size;
    std::byte*            m_mappedFile;
};

// implementation

constexpr RomFile::RomFile(std::filesystem::path path)
    : m_romFilePath(path),
      m_size(std::filesystem::file_size(m_romFilePath)),
      m_mappedFile(Util::memMapFile(path)) {}

constexpr RomFile::~RomFile() {
    Util::memUnmapFile(m_romFilePath, const_cast<std::byte*>(m_mappedFile));
}

constexpr auto RomFile::readHeader() const -> N64RomHeader {
    N64RomHeader header;
    std::memcpy(&header, m_mappedFile, sizeof(header));
    if constexpr (Util::isLittleEndian()) {
        Util::byteswapMembers(&header);
    }
    return header;
}

template <std::integral T>
constexpr auto RomFile::read(uint64_t addr) const -> T {
    T data;
    std::memcpy(&data, m_mappedFile + addr, sizeof(T));
    if constexpr (Util::isLittleEndian()) {
        data = std::byteswap(data);
    }
    return data;
}

constexpr auto RomFile::size() const -> std::size_t {
    return m_size;
}

constexpr auto RomFile::data() const
    -> std::byte* {
    return m_mappedFile;
}