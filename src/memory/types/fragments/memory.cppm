module;
#include <util/defines.hpp>
export module MemoryTypes:Memory;

import std;
import Util;

import :Segments;

namespace Memory {
export class Memory {
  public:
    Memory(std::shared_ptr<Util::Logger> logger, std::size_t memorySize)
        : m_logger(logger),
          m_memory(reinterpret_cast<std::byte*>(std::malloc(memorySize))) {}

    template <std::integral T>
    auto read(PhysicalAddr addr) const -> T;

    template <std::integral T>
    auto write(PhysicalAddr addr, T data) const -> void;

    template <std::size_t N>
    auto memcpy(PhysicalAddr dst, PhysicalAddr src) const -> void {
        std::memcpy(m_memory + dst, m_memory + src, N);
    }

  private:
    std::shared_ptr<Util::Logger> m_logger;
    std::byte*                    m_memory{};
};

template <std::integral T>
auto Memory::read(PhysicalAddr paddr) const -> T {
    auto data = T{};
    std::memcpy(&data, m_memory + paddr, sizeof(T));
    data = Util::byteswapIfLittleEndian(data);

    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::PHYS_MEM>(
            std::tuple{"op", "read"},
            std::tuple{"size", sizeof(T)},
            std::tuple{"addr", HEXFMT32, paddr},
            std::tuple{"data", HEXFMT32, static_cast<std::make_unsigned_t<T>>(data)});
    }
    return data;
}

template <std::integral T>
auto Memory::write(PhysicalAddr paddr, T data) const -> void {
    IF_LOG_ENABLED(m_logger) {
        const auto printData = static_cast<std::make_unsigned_t<T>>(data);
        m_logger->log<Level::HIGH, Sys::PHYS_MEM>(
            std::tuple{"op", "write"},
            std::tuple{"size", sizeof(T)},
            std::tuple{"addr", HEXFMT32, paddr},
            std::tuple{"data", HEXFMT32, printData});
    }
    data = Util::byteswapIfLittleEndian(data);
    std::memcpy(m_memory + paddr, &data, sizeof(T));
}

} // namespace Memory