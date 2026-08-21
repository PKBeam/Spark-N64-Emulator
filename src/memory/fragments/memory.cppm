module;

#include <util/defines.hpp>

export module Memory:Memory;

import std;
import Interfaces;
import Rom;
import Util;

import :Segments;

namespace Memory {
export class Memory {
  public:
    Memory(std::shared_ptr<Util::Logger> logger, std::byte* memory) : m_logger(logger), m_hostMemory(memory) {}

    template <std::integral T>
    auto read(VirtualAddr addr) const -> T;

    template <std::integral T>
    auto write(VirtualAddr addr, T data) const -> void;

    auto data() const -> void*;

    template <std::integral T = std::byte>
    auto translate(VirtualAddr vaddr) const -> PhysicalAddr;

    auto registerAudioInterface(Interfaces::Interface* interface) -> void //
        pre(interface != nullptr);

    auto registerMipsInterface(Interfaces::Interface* interface) -> void //
        pre(interface != nullptr);

    auto registerRdramInterface(Interfaces::Interface* interface) -> void //
        pre(interface != nullptr);

    auto registerRspRegisters(Interfaces::Interface* interface) -> void //
        pre(interface != nullptr);

    auto registerPeripheralInterface(Interfaces::Interface* interface) -> void //
        pre(interface != nullptr);

    auto registerSerialInterface(Interfaces::Interface* interface) -> void //
        pre(interface != nullptr);

    auto registerVideoInterface(Interfaces::Interface* interface) -> void //
        pre(interface != nullptr);

  private:
    std::shared_ptr<Util::Logger> m_logger;
    std::byte*                    m_hostMemory;

    Interfaces::Interface* m_audioInterface;
    Interfaces::Interface* m_mipsInterface;
    Interfaces::Interface* m_rdramInterface;
    Interfaces::Interface* m_rspRegisters;
    Interfaces::Interface* m_peripheralInterface;
    Interfaces::Interface* m_serialInterface;
    Interfaces::Interface* m_videoInterface;
};

namespace Impl {
auto getPhysicalSegment(PhysicalAddr paddr) -> PhysSeg {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^PhysSeg)) {
        constexpr auto a     = std::meta::annotations_of_with_type(e, ^^Util::Range)[0];
        constexpr auto range = std::meta::extract<Util::Range>(a);
        if (range.contains(paddr)) {
            return [:e:];
        }
    }
    throw Util::Error("Translation failed on N64 physical address {:#08x}", paddr);
}
} // namespace Impl

template <std::integral T>
auto Memory::read(VirtualAddr addr) const -> T { // TODO improve performance
    const auto paddr    = translate<T>(addr);
    const auto hostAddr = m_hostMemory + paddr;

    T data{};

    switch (Impl::getPhysicalSegment(paddr)) {
        // these are all typical "memory" spaces
        case PhysSeg::RDRAM: [[fallthrough]];
        case PhysSeg::RSP_DMEM: [[fallthrough]];
        case PhysSeg::RSP_IMEM:
            std::memcpy(&data, hostAddr, sizeof(T));
            Util::byteswapIfLittleEndian(data);
            break;
        case PhysSeg::RDRAM_REG: {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Util::Verbosity::MED>(std::tuple{"warning", "Ignoring RDRAM register read"});
            }
            break;
        }
        case PhysSeg::RSP_REG: {
            data = m_rspRegisters->sizedRead(paddr, sizeof(T));
            break;
        }
        case PhysSeg::MIPS_INTERFACE: data = m_mipsInterface->sizedRead(paddr, sizeof(T)); break;
        case PhysSeg::AUDIO_INTERFACE: data = m_audioInterface->sizedRead(paddr, sizeof(T)); break;
        case PhysSeg::VIDEO_INTERFACE: data = m_videoInterface->sizedRead(paddr, sizeof(T)); break;
        case PhysSeg::PERIPHERAL_INTERFACE: data = m_peripheralInterface->sizedRead(paddr, sizeof(T)); break;
        case PhysSeg::RDRAM_INTERFACE: data = m_rdramInterface->sizedRead(paddr, sizeof(T)); break;
        case PhysSeg::SERIAL_INTERFACE: data = m_serialInterface->sizedRead(paddr, sizeof(T)); break;
        case PhysSeg::PI_BUS:
            data = dynamic_cast<Interfaces::PeripheralInterface*>(m_peripheralInterface)->readBus<T>(paddr);
            break;
        case PhysSeg::SI_BUS:
            data = dynamic_cast<Interfaces::SerialInterface*>(m_serialInterface)->readBus<T>(paddr);
            break;
        default:
            throw Util::Error(
                "Unimplemented physical memory range {}", Util::enumName(Impl::getPhysicalSegment(paddr)).value_or("Unknown"));
    }

    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"size", sizeof(T)},
            std::tuple{"vAddr", "0x{:08x}", addr},
            std::tuple{"pAddr", "0x{:08x}", paddr},
            std::tuple{"ptr", "{:p}", (void*)hostAddr},
            std::tuple{"data", "0x{:08x}", static_cast<std::make_unsigned_t<T>>(data)});
    }
    return data;
}

template <std::integral T>
auto Memory::write(VirtualAddr addr, T data) const -> void {
    const auto paddr    = translate<T>(addr);
    const auto hostAddr = m_hostMemory + paddr;

    switch (Impl::getPhysicalSegment(paddr)) {
        case PhysSeg::RDRAM: [[fallthrough]];
        case PhysSeg::RSP_DMEM: [[fallthrough]];
        case PhysSeg::RSP_IMEM:
            Util::byteswapIfLittleEndian(data);
            std::memcpy(hostAddr, &data, sizeof(T));
            break;
        case PhysSeg::MIPS_INTERFACE: m_mipsInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::AUDIO_INTERFACE: m_audioInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::VIDEO_INTERFACE: m_videoInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::RDRAM_INTERFACE: m_rdramInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::RDRAM_REG: {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Util::Verbosity::MED>(std::tuple{"warning", "Ignoring RDRAM register write"});
            }
            break;
        }
        case PhysSeg::RSP_REG: m_rspRegisters->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::PERIPHERAL_INTERFACE: m_peripheralInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::SERIAL_INTERFACE: m_serialInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::PI_BUS:
            data = dynamic_cast<Interfaces::PeripheralInterface*>(m_peripheralInterface)->readBus<T>(paddr);
            break;
        case PhysSeg::SI_BUS:
            data = dynamic_cast<Interfaces::SerialInterface*>(m_serialInterface)->readBus<T>(paddr);
            break;
        default:
            throw Util::Error(
                "Unimplemented physical memory range {}", Util::enumName(Impl::getPhysicalSegment(paddr)).value_or("Unknown"));
    }

    IF_LOG_ENABLED(m_logger) {
        auto printData = data;
        switch (Impl::getPhysicalSegment(paddr)) {
            case PhysSeg::RDRAM: [[fallthrough]];
            case PhysSeg::RSP_DMEM: [[fallthrough]];
            case PhysSeg::RSP_IMEM:
                Util::byteswapIfLittleEndian(printData);
            default: break;
        }
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"size", sizeof(T)},
            std::tuple{"vAddr", "0x{:08x}", addr},
            std::tuple{"pAddr", "0x{:08x}", paddr},
            std::tuple{"ptr", "{:p}", (void*)hostAddr},
            std::tuple{"data", "0x{:08x}", static_cast<std::make_unsigned_t<T>>(printData)});
    }
}

template <std::integral T>
auto Memory::translate(VirtualAddr vaddr) const -> PhysicalAddr {
    if (vaddr % sizeof(T) != 0) {
        throw Util::Error("Unaligned N64 virtual address access {:#08x}, size {}", vaddr, sizeof(T));
    }

    template for (constexpr auto e : Util::staticEnumeratorsOf(^^VirtSeg)) {
        constexpr auto a     = std::meta::annotations_of_with_type(e, ^^Util::Range)[0];
        constexpr auto range = std::meta::extract<Util::Range>(a);
        if (range.contains(vaddr)) {
            if constexpr (e != (^^VirtSeg::KSEG0) && e != ^^VirtSeg::KSEG1) {
                throw Util::Error(
                    "Unimplemented virtual memory range {}", std::meta::identifier_of(e));
            }
            if (!range.contains(vaddr + sizeof(T) - 1)) {
                throw Util::Error("Out of bounds N64 virtual address access {:#08x}, size {}", vaddr, sizeof(T));
            }
            return vaddr - range.lower;
        }
    }
    throw Util::Error("Translation failed on N64 virtual address {:#08x}", vaddr);
}

auto Memory::registerAudioInterface(Interfaces::Interface* interface) -> void {
    m_audioInterface = interface;
}

auto Memory::registerMipsInterface(Interfaces::Interface* interface) -> void {
    m_mipsInterface = interface;
}

auto Memory::registerRdramInterface(Interfaces::Interface* interface) -> void {
    m_rdramInterface = interface;
}

auto Memory::registerRspRegisters(Interfaces::Interface* interface) -> void {
    m_rspRegisters = interface;
}

auto Memory::registerPeripheralInterface(Interfaces::Interface* interface) -> void {
    m_peripheralInterface = interface;
}

auto Memory::registerSerialInterface(Interfaces::Interface* interface) -> void {
    m_serialInterface = interface;
}

auto Memory::registerVideoInterface(Interfaces::Interface* interface) -> void {
    m_videoInterface = interface;
}

} // namespace Memory