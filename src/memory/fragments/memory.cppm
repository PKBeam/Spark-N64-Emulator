export module Memory:Memory;

import std;
import Interfaces;
import Rom;
import Util;

import :Segments;

extern "C" {
auto readMipsInterface(void*, uint32_t, std::size_t) -> uint32_t;
auto writeMipsInterface(void*, uint32_t, uint64_t) -> void;
}

namespace Memory {
export class Memory {
  public:
    Memory(std::shared_ptr<Util::Logger> logger, std::byte* memory) : m_logger(logger), m_hostMemory(memory) {}

    auto registerMipsInterface(Interfaces::MmioRegisters* interface) -> void;
    auto registerRdramInterface(Interfaces::MmioRegisters* interface) -> void;
    auto registerRspRegisters(Interfaces::MmioRegisters* interface) -> void;
    auto registerPeripheralInterface(Interfaces::MmioRegisters* interface) -> void;
    auto registerSerialInterface(Interfaces::MmioRegisters* interface) -> void;

    template <std::integral T>
    auto read(VirtualAddr addr) const -> T;

    template <std::integral T>
        requires(sizeof(T) <= 4) // 64-bit writes need to be split for now
    auto write(VirtualAddr addr, uint32_t data) const -> void;

    auto data() const -> void*;

    auto translate(VirtualAddr vaddr, std::size_t dataSize = 0) const -> PhysicalAddr;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    std::byte*                    m_hostMemory;

    Interfaces::MmioRegisters* m_mipsInterface;
    Interfaces::MmioRegisters* m_rdramInterface;
    Interfaces::MmioRegisters* m_rspRegisters;
    Interfaces::MmioRegisters* m_peripheralInterface;
    Interfaces::MmioRegisters* m_serialInterface;
};

namespace Impl {
auto getPhysicalSegment(PhysicalAddr paddr) -> PhysSeg {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^PhysSeg)) {
        constexpr auto a     = std::meta::annotations_of_with_type(e, ^^Util::Range)[0];
        constexpr auto range = std::meta::extract<Util::Range>(a);
        if (range.contains(paddr)) {
            auto enumName = std::meta::identifier_of(e);
            return [:e:];
        }
    }
    throw std::runtime_error(std::format(
        "Translation failed on N64 physical address {:#08x}", paddr));
}
} // namespace Impl

template <std::integral T>
auto Memory::read(VirtualAddr addr) const -> T {
    const auto paddr    = translate(addr, sizeof(T));
    const auto hostAddr = m_hostMemory + paddr;

    T data{};

    switch (Impl::getPhysicalSegment(paddr)) {
        // these are all typical "memory" spaces
        case PhysSeg::RDRAM:
            [[fallthrough]];
        case PhysSeg::RSP_DMEM:
            [[fallthrough]];
        case PhysSeg::RSP_IMEM:
            std::memcpy(&data, hostAddr, sizeof(T));
            break;
        case PhysSeg::RDRAM_REG: {
            if (m_logger && m_logger->enabled()) {
                m_logger->log<Util::Verbosity::MED>(std::tuple{"warning", "Ignoring RDRAM register read"});
            }
            break;
        }
        case PhysSeg::RSP_REG: {
            data = m_rspRegisters->sizedRead(paddr, sizeof(T));
            break;
        }
        case PhysSeg::MIPS_INTERFACE: {
            data = m_mipsInterface->sizedRead(paddr, sizeof(T));
            break;
        }
        case PhysSeg::PERIPHERAL_INTERFACE: {
            data = m_peripheralInterface->sizedRead(paddr, sizeof(T));
            break;
        }
        case PhysSeg::RDRAM_INTERFACE: {
            data = m_rdramInterface->sizedRead(paddr, sizeof(T));
            break;
        }
        case PhysSeg::SERIAL_INTERFACE: {
            data = m_serialInterface->sizedRead(paddr, sizeof(T));
            break;
        }
        case PhysSeg::PI_BUS:
            data = dynamic_cast<Interfaces::PeripheralInterface*>(m_peripheralInterface)->readBus<T>(paddr);
            break;
        case PhysSeg::SI_BUS:
            data = dynamic_cast<Interfaces::SerialInterface*>(m_serialInterface)->readBus<T>(paddr);
            break;
        default:
            throw std::runtime_error(std::format(
                "Unimplemented physical memory range {}", Util::enumName(Impl::getPhysicalSegment(paddr)).value_or("Unknown")));
    }

    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"n64VAddr", "0x{:08x}", addr},
            std::tuple{"n64PAddr", "0x{:08x}", paddr},
            std::tuple{"hostAddr", "{:p}", (void*)hostAddr},
            std::tuple{"data", "0x{:08x}", data},
            std::tuple{"size", sizeof(T)});
    }
    return data;
}

template <std::integral T>
    requires(sizeof(T) <= 4)
auto Memory::write(VirtualAddr addr, uint32_t data) const -> void {
    const auto paddr    = translate(addr, sizeof(T));
    const auto hostAddr = m_hostMemory + paddr;

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
        case PhysSeg::RSP_REG: {
            m_rspRegisters->sizedWrite(paddr, sizeof(T), data);
            break;
        }
        case PhysSeg::PERIPHERAL_INTERFACE: {
            m_peripheralInterface->sizedWrite(paddr, sizeof(T), data);
            break;
        }
        case PhysSeg::SERIAL_INTERFACE: {
            m_serialInterface->sizedWrite(paddr, sizeof(T), data);
            break;
        }
        case PhysSeg::RDRAM:
            [[fallthrough]];
        case PhysSeg::RSP_DMEM:
            [[fallthrough]];
        case PhysSeg::RSP_IMEM:
            std::memcpy(hostAddr, &data, sizeof(T));
            break;
        case PhysSeg::PI_BUS:
            data = dynamic_cast<Interfaces::PeripheralInterface*>(m_peripheralInterface)->readBus<T>(paddr);
            break;
        case PhysSeg::SI_BUS:
            data = dynamic_cast<Interfaces::SerialInterface*>(m_serialInterface)->readBus<T>(paddr);
            break;
        default:
            throw std::runtime_error(std::format(
                "Unimplemented physical memory range {}", Util::enumName(Impl::getPhysicalSegment(paddr)).value_or("Unknown")));
    }

    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"n64VAddr", "0x{:08x}", addr},
            std::tuple{"n64PAddr", "0x{:08x}", paddr},
            std::tuple{"hostAddr", "{:p}", (void*)hostAddr},
            std::tuple{"data", "0x{:08x}", data},
            std::tuple{"size", sizeof(T)});
    }
}

auto Memory::translate(VirtualAddr vaddr, std::size_t dataSize) const -> PhysicalAddr {
    if (vaddr % dataSize != 0) {
        throw std::runtime_error(std::format(
            "Unaligned N64 virtual address access {:#08x}, size {}", vaddr, dataSize));
    }

    template for (constexpr auto e : Util::staticEnumeratorsOf(^^VirtSeg)) {
        constexpr auto a     = std::meta::annotations_of_with_type(e, ^^Util::Range)[0];
        constexpr auto range = std::meta::extract<Util::Range>(a);
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

auto Memory::registerMipsInterface(Interfaces::MmioRegisters* interface) -> void {
    m_mipsInterface = interface;
}

auto Memory::registerRdramInterface(Interfaces::MmioRegisters* interface) -> void {
    m_rdramInterface = interface;
}

auto Memory::registerRspRegisters(Interfaces::MmioRegisters* interface) -> void {
    m_rspRegisters = interface;
}

auto Memory::registerPeripheralInterface(Interfaces::MmioRegisters* interface) -> void {
    m_peripheralInterface = interface;
}

auto Memory::registerSerialInterface(Interfaces::MmioRegisters* interface) -> void {
    m_serialInterface = interface;
}

} // namespace Memory