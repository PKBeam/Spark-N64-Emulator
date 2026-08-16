export module Interfaces:PeripheralInterface;

import std;
import Rom;
import Util;

import :MmioRegisters;
import :PeripheralInterfaceTypes;

namespace Interfaces {

export class PeripheralInterface : public MmioRegisters {
    using RegisterPtr = std::variant<PI_DRAM_ADDR*, PI_CART_ADDR*, PI_RD_LEN*, PI_WR_LEN*, PI_STATUS*, PI_BSD_DOM1_LAT*, PI_BSD_DOM1_PWD*, PI_BSD_DOM1_PGS*, PI_BSD_DOM1_RLS*, PI_BSD_DOM2_LAT*, PI_BSD_DOM2_PWD*, PI_BSD_DOM2_PGS*, PI_BSD_DOM2_RLS*>;

  public:
    PeripheralInterface(std::shared_ptr<Util::Logger> logger, std::byte* memory) : m_logger(logger), m_memory(memory) {}

    auto loadRom(const RomFile* rom) -> void //
        pre(rom != nullptr);

    auto read(uint32_t addr) -> uint32_t override;

    auto write(uint32_t addr, uint32_t data) -> void override;

    template <std::integral T>
    auto readBus(uint32_t addr) -> T;

    template <std::integral T>
    auto writeBus(uint32_t addr, T data) -> void;

    auto dmaMemcpy(uint32_t dest, uint32_t src, std::size_t len) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    const RomFile*                m_romFile;
    std::byte*                    m_memory;
    PI_STATUS                     m_status{};
    uint32_t                      m_dramAddr = 0;
    uint32_t                      m_cartAddr = 0;

    auto readRegister(uint32_t addr) -> uint32_t;
    auto writeRegister(uint32_t addr, uint32_t data) -> void;
};

auto openBusRead(uint32_t addr) -> uint32_t {
    return (addr << 16) | (addr & 0xFFFF);
}

auto PeripheralInterface::loadRom(const RomFile* rom) -> void {
    m_romFile = rom;
}

template <std::integral T>
auto PeripheralInterface::readBus(uint32_t addr) -> T {
    const auto [e, range] = Util::getRange<PiDmaRanges>(addr);
    switch (e) {
        case PiDmaRanges::RDRAM: {
            T data{};
            std::memcpy(&data, m_memory + addr, sizeof(T));
            return data;
        }
        case PiDmaRanges::PI_REG:
            throw std::runtime_error("PI regs must not be accessed via the bus");
        case PiDmaRanges::SRAM:
            throw std::runtime_error("SRAM not implemented");
        case PiDmaRanges::ROM:
            m_dramAddr += sizeof(T);
            m_cartAddr += sizeof(T);
            return m_romFile ? m_romFile->read<T>(addr - range.lower) : openBusRead(addr);
        case PiDmaRanges::N64DD_CTRL_REG: [[fallthrough]];
        case PiDmaRanges::N64DD_IPL_ROM:
            if (m_logger && m_logger->enabled()) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", "PI"},
                    std::tuple{"warning", "Ignoring read from N64DD memory range"});
            }
            break;
    }
    return openBusRead(addr);
}

template <std::integral T>
auto PeripheralInterface::writeBus(uint32_t addr, T data) -> void {
    const auto [e, range] = Util::getRange<PiDmaRanges>(addr);
    switch (e) {
        case PiDmaRanges::RDRAM:
            std::memcpy(m_memory + addr, &data, sizeof(T));
            return;
        case PiDmaRanges::PI_REG:
            throw std::runtime_error("PI regs must not be accessed via the bus");
        case PiDmaRanges::SRAM:
            throw std::runtime_error("SRAM not implemented");
        case PiDmaRanges::ROM:
            return; // ignore
        case PiDmaRanges::N64DD_CTRL_REG: [[fallthrough]];
        case PiDmaRanges::N64DD_IPL_ROM:
            if (m_logger && m_logger->enabled()) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", "PI"},
                    std::tuple{"warning", "Ignoring write to N64DD memory range"});
            }
            return; // ignore
    }
}

auto PeripheralInterface::read(uint32_t addr) -> uint32_t {
    contract_assert(PI_REG_ADDR::BASE <= addr && addr <= PI_REG_ADDR::END);
    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case PI_REG_ADDR::PI_DRAM_ADDR: return m_dramAddr;
            case PI_REG_ADDR::PI_CART_ADDR: return m_cartAddr;
            case PI_REG_ADDR::PI_RD_LEN: return 0x7F;
            case PI_REG_ADDR::PI_WR_LEN: return 0x7F;
            case PI_REG_ADDR::PI_STATUS: return std::bit_cast<uint32_t>(m_status);
            case PI_REG_ADDR::PI_BSD_DOM1_LAT: return std::bit_cast<uint32_t>(PI_BSD_DOM1_LAT{});
            case PI_REG_ADDR::PI_BSD_DOM1_PWD: return std::bit_cast<uint32_t>(PI_BSD_DOM1_PWD{});
            case PI_REG_ADDR::PI_BSD_DOM1_PGS: return std::bit_cast<uint32_t>(PI_BSD_DOM1_PGS{});
            case PI_REG_ADDR::PI_BSD_DOM1_RLS: return std::bit_cast<uint32_t>(PI_BSD_DOM1_RLS{});
            case PI_REG_ADDR::PI_BSD_DOM2_LAT: return std::bit_cast<uint32_t>(PI_BSD_DOM2_LAT{});
            case PI_REG_ADDR::PI_BSD_DOM2_PWD: return std::bit_cast<uint32_t>(PI_BSD_DOM2_PWD{});
            case PI_REG_ADDR::PI_BSD_DOM2_PGS: return std::bit_cast<uint32_t>(PI_BSD_DOM2_PGS{});
            case PI_REG_ADDR::PI_BSD_DOM2_RLS: return std::bit_cast<uint32_t>(PI_BSD_DOM2_RLS{});
            default:
                throw std::runtime_error(std::format("No PI register found for addr {:#08x}", addr));
        };
    };
    auto value = readReg(addr);
    if (m_logger && m_logger->enabled()) {
        auto name = Util::enumName(static_cast<decltype(PI_REG_ADDR::PI_DRAM_ADDR)>(addr)).value_or("Unknown");
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", "PI"},
            std::tuple{"op", "read"},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", "0x{:08x}", value});
    }
    return value;
}

auto PeripheralInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(PI_REG_ADDR::BASE <= addr && addr <= PI_REG_ADDR::END);

    if (m_logger && m_logger->enabled()) {
        auto name = Util::enumName(static_cast<decltype(PI_REG_ADDR::PI_DRAM_ADDR)>(addr)).value_or("Unknown");
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", "PI"},
            std::tuple{"op", "write"},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", "0x{:08x}", data});
    }
    switch (addr) {
        case PI_REG_ADDR::PI_DRAM_ADDR:
            m_dramAddr = data;
            if (m_logger && m_logger->enabled() && data % 8 != 0) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", "PI"},
                    std::tuple{"warning", "PI DMA RDRAM address not aligned to 8-byte boundary"});
            }
            break;
        case PI_REG_ADDR::PI_CART_ADDR:
            m_cartAddr = data;
            break;
        case PI_REG_ADDR::PI_RD_LEN: {
            const auto status = std::bit_cast<PI_RD_LEN>(data);
            const auto length = status.rdLen_23_0 + 1;
            dmaMemcpy(m_dramAddr, m_cartAddr, length);
            return;
        }
        case PI_REG_ADDR::PI_WR_LEN: {
            const auto status = std::bit_cast<PI_WR_LEN>(data);
            const auto length = status.wrLen_23_0 + 1;
            dmaMemcpy(m_cartAddr, m_dramAddr, length);
            return;
        }
        case PI_REG_ADDR::PI_STATUS: {
            const auto status = std::bit_cast<PI_STATUS::PI_STATUS_write>(data);
            // DMAs are modelled as instant, so ignore dmaReset
            if (status.clearInterrupt) {
                m_status.interrupt = 0;
            }
            break;
        }
        case PI_REG_ADDR::PI_BSD_DOM1_LAT: [[fallthrough]];
        case PI_REG_ADDR::PI_BSD_DOM1_PWD: [[fallthrough]];
        case PI_REG_ADDR::PI_BSD_DOM1_PGS: [[fallthrough]];
        case PI_REG_ADDR::PI_BSD_DOM1_RLS: [[fallthrough]];
        case PI_REG_ADDR::PI_BSD_DOM2_LAT: [[fallthrough]];
        case PI_REG_ADDR::PI_BSD_DOM2_PWD: [[fallthrough]];
        case PI_REG_ADDR::PI_BSD_DOM2_PGS: [[fallthrough]];
        case PI_REG_ADDR::PI_BSD_DOM2_RLS:
            if (m_logger && m_logger->enabled()) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", "PI"},
                    std::tuple{"warning", "Ignoring PI_BSD_* write"});
            }
            break;
        default:
            throw std::runtime_error(std::format("No PI register found for addr {:#08x}", addr));
    };
}

auto PeripheralInterface::dmaMemcpy(uint32_t dst, uint32_t src, std::size_t len) -> void {
    if (m_logger && m_logger->enabled() && len % 2 != 0) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", "PI"},
            std::tuple{"warning", "PI DMA length not an even number"});
    }

    for (auto i = 0u; i < len; ++i) {
        const auto byte = readBus<uint8_t>(src + i);
        writeBus<uint8_t>(dst + i, byte);
    }
    m_cartAddr += len;
    m_dramAddr += len;

    m_status.dmaBusy   = 0;
    m_status.ioBusy    = 0;
    m_status.dmaError  = 0;
    m_status.interrupt = 1;
}

} // namespace Interfaces