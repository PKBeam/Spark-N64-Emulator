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

    ~Memory();

    template <std::integral T>
    auto read(PhysicalAddr addr) const -> T;

    template <std::integral T>
    auto write(PhysicalAddr addr, T data) const -> void;

    template <std::unsigned_integral T>
    auto memcpy(PhysicalAddr dst, PhysicalAddr src) const -> void {
        IF_LOG_ENABLED(m_logger) {
            const auto data = *reinterpret_cast<const T*>(m_memory + src);
            m_logger->log<Level::HIGH, Sys::PHYS_MEM>(
                std::tuple{"op", "rw"},
                std::tuple{"src", HEXFMT32, src},
                std::tuple{"dst", HEXFMT32, dst},
                makePrintData(Util::byteswapIfLittleEndian(data)));
        }
        std::memcpy(m_memory + dst, m_memory + src, sizeof(T));
    }

  private:
    std::shared_ptr<Util::Logger> m_logger;
    std::byte*                    m_memory{};
};

Memory::~Memory() {
    std::free(reinterpret_cast<void*>(m_memory));
}

template <std::integral T>
auto Memory::read(PhysicalAddr paddr) const -> T {
    auto data = T{};
    std::memcpy(&data, m_memory + paddr, sizeof(T));
    data = Util::byteswapIfLittleEndian(data);

    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::PHYS_MEM>(
            std::tuple{"op", "r"},
            std::tuple{"addr", HEXFMT32, paddr},
            makePrintData(data));
    }
    return data;
}

template <std::integral T>
auto Memory::write(PhysicalAddr paddr, T data) const -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::PHYS_MEM>(
            std::tuple{"op", "w"},
            std::tuple{"addr", HEXFMT32, paddr},
            makePrintData(data));
    }
    data = Util::byteswapIfLittleEndian(data);
    std::memcpy(m_memory + paddr, &data, sizeof(T));
}

} // namespace Memory