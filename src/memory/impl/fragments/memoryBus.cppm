module;
#include <util/defines.hpp>
export module Memory:MemoryBus;

import std;
import Interfaces;
import MemoryTypes;
import Rom;
import Util;

namespace Memory {
export class MemoryBus {
  public:
    MemoryBus(std::shared_ptr<Util::Logger> logger, ::Memory::Memory* memory) : m_logger(logger), m_memory(memory) {}

    template <std::integral T = std::byte>
    auto translate(VirtualAddr vaddr) const -> PhysicalAddr;

    template <std::integral T>
    auto readPhysical(PhysicalAddr addr) const -> T;

    template <std::integral T>
    auto writePhysical(PhysicalAddr addr, T data) const -> void;

    template <std::integral T>
    auto read(VirtualAddr addr) const -> T;

    template <std::integral T>
    auto write(VirtualAddr addr, T data) const -> void;

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
    ::Memory::Memory*             m_memory{};

    Interfaces::Interface* m_audioInterface{};
    Interfaces::Interface* m_mipsInterface{};
    Interfaces::Interface* m_rdramInterface{};
    Interfaces::Interface* m_rspRegisters{};
    Interfaces::Interface* m_peripheralInterface{};
    Interfaces::Interface* m_serialInterface{};
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
    throw Util::Error("Translation failed on N64 physical address " HEXFMT32, paddr);
}

template <std::integral T>
auto translate(VirtualAddr vaddr, std::shared_ptr<Util::Logger> logger) -> PhysicalAddr {
    IF_LOG_ENABLED(logger) {
        if (vaddr % sizeof(T) != 0) {
            logger->log<Level::HIGH, Sev::WARNING, Sys::RDRAM>("Unaligned virtual address access " HEXFMT32 ", size {}", vaddr, sizeof(T));
        }
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
                throw Util::Error("Out of bounds N64 virtual address access " HEXFMT32 ", size {}", vaddr, sizeof(T));
            }
            return vaddr - range.lower;
        }
    }
    throw Util::Error("Translation failed on N64 virtual address " HEXFMT32, vaddr);
}
} // namespace Impl

template <std::integral T>
auto MemoryBus::read(VirtualAddr addr) const -> T {
    return readPhysical<T>(Impl::translate<T>(addr, m_logger));
}

template <std::integral T>
auto MemoryBus::write(VirtualAddr addr, T data) const -> void {
    writePhysical<T>(Impl::translate<T>(addr, m_logger), data);
}

template <std::integral T>
auto MemoryBus::readPhysical(PhysicalAddr paddr) const -> T {
    auto data = T{};
    switch (Impl::getPhysicalSegment(paddr)) {
        // these are all typical "memory" spaces
        case PhysSeg::RDRAM: [[fallthrough]];
        case PhysSeg::RSP_DMEM: [[fallthrough]];
        case PhysSeg::RSP_IMEM:
            data = m_memory->read<T>(paddr);
            break;
        case PhysSeg::RDRAM_UNUSED:
            data = 0;
            break;
        case PhysSeg::RDRAM_REG: {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RDRAM>("Ignoring RDRAM register read");
            }
            break;
        }
        case PhysSeg::DPC_REG: {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RDRAM>("Ignoring RDP command register read");
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
            throw Util::Error("Unimplemented physical memory range {}", Impl::getPhysicalSegment(paddr));
    }

    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::RDRAM>(
            std::tuple{"op", "r"},
            std::tuple{"addr", HEXFMT32, paddr},
            makePrintData(data));
    }
    return data;
}

template <std::integral T>
auto MemoryBus::writePhysical(PhysicalAddr paddr, T data) const -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::RDRAM>(
            std::tuple{"op", "w"},
            std::tuple{"addr", HEXFMT32, paddr},
            makePrintData(data));
    }
    switch (Impl::getPhysicalSegment(paddr)) {
        case PhysSeg::RDRAM: [[fallthrough]];
        case PhysSeg::RSP_DMEM: [[fallthrough]];
        case PhysSeg::RSP_IMEM:
            m_memory->write<T>(paddr, data);
            break;
        case PhysSeg::RDRAM_UNUSED: break;
        case PhysSeg::MIPS_INTERFACE: m_mipsInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::AUDIO_INTERFACE: m_audioInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::VIDEO_INTERFACE: m_videoInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::RDRAM_INTERFACE: m_rdramInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::RDRAM_REG: {
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::HIGH, Sev::WARNING, Sys::RDRAM>("Ignoring RDRAM register write");
            }
            break;
        }
        case PhysSeg::RSP_REG: m_rspRegisters->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::PERIPHERAL_INTERFACE: m_peripheralInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::SERIAL_INTERFACE: m_serialInterface->sizedWrite(paddr, sizeof(T), data); break;
        case PhysSeg::PI_BUS:
            dynamic_cast<Interfaces::PeripheralInterface*>(m_peripheralInterface)->writeBus<T>(paddr, data);
            break;
        case PhysSeg::SI_BUS:
            dynamic_cast<Interfaces::SerialInterface*>(m_serialInterface)->writeBus<T>(paddr, data);
            break;
        default:
            throw Util::Error("Unimplemented physical memory range {}", Impl::getPhysicalSegment(paddr));
    }
}

auto MemoryBus::registerAudioInterface(Interfaces::Interface* interface) -> void {
    m_audioInterface = interface;
}

auto MemoryBus::registerMipsInterface(Interfaces::Interface* interface) -> void {
    m_mipsInterface = interface;
}

auto MemoryBus::registerRdramInterface(Interfaces::Interface* interface) -> void {
    m_rdramInterface = interface;
}

auto MemoryBus::registerRspRegisters(Interfaces::Interface* interface) -> void {
    m_rspRegisters = interface;
}

auto MemoryBus::registerPeripheralInterface(Interfaces::Interface* interface) -> void {
    m_peripheralInterface = interface;
}

auto MemoryBus::registerSerialInterface(Interfaces::Interface* interface) -> void {
    m_serialInterface = interface;
}

auto MemoryBus::registerVideoInterface(Interfaces::Interface* interface) -> void {
    m_videoInterface = interface;
}

} // namespace Memory