export module Memory:Memory;

import std;
import Interfaces;
import Rom;
import Util;

import :Types;

extern "C" {
auto readMipsInterface(void*, uint32_t, std::size_t) -> uint32_t;
auto writeMipsInterface(void*, uint32_t, uint64_t) -> void;
}

namespace Memory {
export class Memory {
  public:
    Memory(
        std::size_t                   size,
        std::shared_ptr<Util::Logger> logger = nullptr);

    ~Memory();

    auto registerMipsInterface(std::unique_ptr<Interfaces::MmioRegisters>&& interface) -> void;
    auto registerRdramInterface(std::unique_ptr<Interfaces::MmioRegisters>&& interface) -> void;

    auto loadRom(const RomFile& rom) -> void;

    template <std::integral T>
    auto read(VirtualAddr addr) const -> T;

    template <std::integral T>
        requires(sizeof(T) <= 4) // 64-bit writes need to be split for now
    auto write(VirtualAddr addr, uint32_t data) const -> void;

    auto data() const -> void*;

    auto translate(VirtualAddr vaddr, std::size_t dataSize = 0) const -> PhysicalAddr;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    void*                         m_hostMemory;
    std::size_t                   m_memorySize;

    std::unique_ptr<Interfaces::MmioRegisters> m_mipsInterface;
    std::unique_ptr<Interfaces::MmioRegisters> m_rdramInterface;
};

namespace Impl {
auto getPhysicalSegment(PhysicalAddr paddr) -> PhysSeg {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^PhysSeg)) {
        constexpr auto a     = std::meta::annotations_of_with_type(e, ^^Range)[0];
        constexpr auto range = std::meta::extract<Range>(a);
        if (range.contains(paddr)) {
            auto enumName = std::meta::identifier_of(e);
            return [:e:];
        }
    }
    throw std::runtime_error(std::format(
        "Translation failed on N64 physical address {:#08x}", paddr));
}
} // namespace Impl

auto Memory::registerMipsInterface(std::unique_ptr<Interfaces::MmioRegisters>&& interface) -> void {
    m_mipsInterface = std::move(interface);
}

auto Memory::registerRdramInterface(std::unique_ptr<Interfaces::MmioRegisters>&& interface) -> void {
    m_rdramInterface = std::move(interface);
}

template <std::integral T>
auto Memory::read(VirtualAddr addr) const -> T {
    const auto paddr    = translate(addr, sizeof(T));
    const auto hostAddr = reinterpret_cast<std::byte*>(m_hostMemory) + paddr;

    T data{};

    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"n64VAddr", "0x{:08x}", addr},
            std::tuple{"n64PAddr", "0x{:08x}", paddr},
            std::tuple{"hostAddr", "{:p}", (void*)hostAddr},
            std::tuple{"data", "0x{:08x}", data},
            std::tuple{"size", sizeof(T)});
    }
    switch (Impl::getPhysicalSegment(paddr)) {
        case PhysSeg::MIPS_INTERFACE: {
            data = m_mipsInterface->sizedRead(paddr, sizeof(T));
            break;
        }
        case PhysSeg::RDRAM_INTERFACE: {
            data = m_rdramInterface->sizedRead(paddr, sizeof(T));
            break;
        }
        case PhysSeg::RDRAM_REG: {
            if (m_logger && m_logger->enabled()) {
                m_logger->log<Util::Verbosity::MED>(std::tuple{"warning", "Ignoring RDRAM register read"});
            }
            break;
        }
        case PhysSeg::RDRAM:
            [[fallthrough]];
        case PhysSeg::RSP_DMEM:
            [[fallthrough]];
        case PhysSeg::RSP_IMEM:
            [[fallthrough]];
        case PhysSeg::SRAM:
            [[fallthrough]];
        case PhysSeg::ROM:
            std::memcpy(&data, hostAddr, sizeof(T));
            break;
        default:
            throw std::runtime_error(std::format(
                "Unimplemented physical memory range {}", Util::enumName(Impl::getPhysicalSegment(paddr)).value_or("Unknown")));
    }
    return data;
}

template <std::integral T>
    requires(sizeof(T) <= 4)
auto Memory::write(VirtualAddr addr, uint32_t data) const -> void {
    const auto paddr    = translate(addr, sizeof(T));
    const auto hostAddr = reinterpret_cast<std::byte*>(m_hostMemory) + paddr;

    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"n64VAddr", "0x{:08x}", addr},
            std::tuple{"n64PAddr", "0x{:08x}", paddr},
            std::tuple{"hostAddr", "{:p}", (void*)hostAddr},
            std::tuple{"data", "0x{:08x}", data},
            std::tuple{"size", sizeof(T)});
    }

    switch (Impl::getPhysicalSegment(paddr)) {
        case PhysSeg::MIPS_INTERFACE: {
            m_mipsInterface->sizedWrite(paddr, sizeof(T), data);
            break;
        }
        case PhysSeg::RDRAM_INTERFACE: {
            m_rdramInterface->sizedWrite(paddr, sizeof(T), data);
            break;
        }
        case PhysSeg::RDRAM_REG: {
            if (m_logger && m_logger->enabled()) {
                m_logger->log<Util::Verbosity::MED>(std::tuple{"warning", "Ignoring RDRAM register write"});
            }
            break;
        }
        case PhysSeg::RDRAM:
            [[fallthrough]];
        case PhysSeg::RSP_DMEM:
            [[fallthrough]];
        case PhysSeg::RSP_IMEM:
            [[fallthrough]];
        case PhysSeg::SRAM:
            [[fallthrough]];
        case PhysSeg::ROM:
            std::memcpy(hostAddr, &data, sizeof(T));
            break;
        default:
            throw std::runtime_error(std::format(
                "Unimplemented physical memory range {}", Util::enumName(Impl::getPhysicalSegment(paddr)).value_or("Unknown")));
    }
}

Memory::Memory(std::size_t size, std::shared_ptr<Util::Logger> logger) {
    auto ptr = std::malloc(size);
    if (!ptr) {
        throw std::runtime_error("Out of memory");
    }
    m_hostMemory = ptr;
    m_memorySize = size;
    m_logger     = logger;
}

Memory::~Memory() {
    std::free(m_hostMemory);
}

auto Memory::loadRom(const RomFile& rom) -> void {
    constexpr auto range = std::meta::extract<Range>(
        std::meta::annotations_of_with_type(^^PhysSeg::ROM, ^^Range)[0]);

    for (auto i = 0uz; i < rom.size(); i += 4) {
        const auto word = rom.read<uint32_t>(i);
        const auto addr = reinterpret_cast<std::byte*>(m_hostMemory) + range.lower + i;
        std::memcpy(addr, &word, 4);

        if (m_logger && m_logger->enabled()) {
            m_logger->log<Util::Verbosity::HIGH>(
                std::tuple{"op", "loadRom"},
                std::tuple{"addr", "0x{:08x}", range.lower + i},
                std::tuple{"hostAddr", "{:p}", (void*)addr},
                std::tuple{"data", "0x{:08x}", word});
        }
    }
}

auto Memory::data() const -> void* {
    return m_hostMemory;
}

auto Memory::translate(VirtualAddr vaddr, std::size_t dataSize) const -> PhysicalAddr {
    if (vaddr % dataSize != 0) {
        throw std::runtime_error(std::format(
            "Unaligned N64 virtual address access {:#08x}, size {}", vaddr, dataSize));
    }

    template for (constexpr auto e : Util::staticEnumeratorsOf(^^VirtSeg)) {
        constexpr auto a     = std::meta::annotations_of_with_type(e, ^^Range)[0];
        constexpr auto range = std::meta::extract<Range>(a);
        if (range.contains(vaddr)) {
            auto enumName = std::meta::identifier_of(e);
            if constexpr (e != (^^VirtSeg::KSEG0) && e != ^^VirtSeg::KSEG1) {
                throw std::runtime_error(std::format(
                    "Unimplemented virtual memory range {}", std::meta::identifier_of(e)));
            }
            if (!range.contains(vaddr + dataSize - 1)) {
                throw std::runtime_error(std::format(
                    "Out of bounds N64 virtual address access {:#08x}, size {}", vaddr, dataSize));
            }
            return vaddr - range.lower;
        }
    }
    throw std::runtime_error(std::format(
        "Translation failed on N64 virtual address {:#08x}", vaddr));
}

} // namespace Memory