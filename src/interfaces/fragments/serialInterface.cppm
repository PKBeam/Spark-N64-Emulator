export module Interfaces:SerialInterface;

import std;
import Util;

import :MmioRegisters;
import :SerialInterfaceTypes;

namespace Interfaces {

export class SerialInterface : public MmioRegisters {
  public:
    SerialInterface(std::shared_ptr<Util::Logger> logger, std::byte* memory) : m_logger(logger), m_memory(memory) {}

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

    template <std::integral T>
    auto readBus(uint32_t addr) -> T;

    template <std::integral T>
    auto writeBus(uint32_t addr, T data) -> void;

    auto dmaMemcpy(uint32_t dst, uint32_t src) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    std::byte*                    m_memory;

    uint32_t  m_dramAddr;
    uint32_t  m_pifAddr;
    SI_STATUS m_status;
};

auto SerialInterface::read(uint32_t addr) -> uint32_t {
    contract_assert(SI_REG_ADDR::BASE <= addr && addr <= SI_REG_ADDR::END);
    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case SI_REG_ADDR::SI_DRAM_ADDR: return m_dramAddr;
            case SI_REG_ADDR::SI_PIF_AD_RD64B: return m_pifAddr;
            case SI_REG_ADDR::SI_PIF_AD_WR4B: [[fallthrough]];
            case SI_REG_ADDR::SI_PIF_AD_WR64B: [[fallthrough]];
            case SI_REG_ADDR::SI_PIF_AD_RD4B:
                return 0;
            case SI_REG_ADDR::SI_STATUS: return std::bit_cast<uint32_t>(m_status);
            default:
                throw std::runtime_error(std::format("No SI register found for addr {:#08x}", addr));
        }
    };

    auto value = readReg(addr);
    if (m_logger && m_logger->enabled()) {
        auto name = Util::enumName(static_cast<decltype(SI_REG_ADDR::SI_DRAM_ADDR)>(addr)).value_or("Unknown");
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", "SI"},
            std::tuple{"op", "read"},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", "0x{:08x}", value});
    }
    return value;
}

auto SerialInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(SI_REG_ADDR::BASE <= addr && addr <= SI_REG_ADDR::END);

    if (m_logger && m_logger->enabled()) {
        auto name = Util::enumName(static_cast<decltype(SI_REG_ADDR::SI_DRAM_ADDR)>(addr)).value_or("Unknown");
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", "SI"},
            std::tuple{"op", "write"},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", "0x{:08x}", data});
    }
    switch (addr) {
        case SI_REG_ADDR::SI_DRAM_ADDR: m_dramAddr = data; break;
        case SI_REG_ADDR::SI_PIF_AD_RD64B: {
            m_pifAddr = data;
            dmaMemcpy(m_dramAddr, m_pifAddr);
            break;
        };
        case SI_REG_ADDR::SI_PIF_AD_WR4B: [[fallthrough]];
        case SI_REG_ADDR::SI_PIF_AD_WR64B: [[fallthrough]];
        case SI_REG_ADDR::SI_PIF_AD_RD4B: break;
        case SI_REG_ADDR::SI_STATUS:
            if (m_logger && m_logger->enabled()) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", "SI"},
                    std::tuple{"warning", "Attempted to write to read-only SI register"});
            }
    }
}

template <std::integral T>
auto SerialInterface::readBus(uint32_t addr) -> T {
    const auto [e, range] = Util::getRange<SiDmaRanges>(addr);
    switch (e) {
        case SiDmaRanges::PIF_ROM: [[fallthrough]];
        case SiDmaRanges::PIF_RAM:
            throw std::runtime_error("SI bus not implemented");
    }
    return 0;
}

template <std::integral T>
auto SerialInterface::writeBus(uint32_t addr, T data) -> void {
    const auto [e, range] = Util::getRange<SiDmaRanges>(addr);
    switch (e) {
        case SiDmaRanges::PIF_ROM: [[fallthrough]];
        case SiDmaRanges::PIF_RAM:
            throw std::runtime_error("SI bus not implemented");
    }
}

auto SerialInterface::dmaMemcpy(uint32_t dst, uint32_t src) -> void {
    constexpr auto len = 64uz;
    for (auto i = 0u; i < len; ++i) {
        const auto byte = readBus<uint8_t>(src + i);
        writeBus<uint8_t>(dst + i, byte);
    }
    m_dramAddr += len - 4;

    m_status.dmaBusy     = 0;
    m_status.ioBusy      = 0;
    m_status.readPending = 0;
    m_status.dmaError    = 0;
    m_status.dmaState    = 0;
    m_status.interrupt   = 1; // todo mirror in MIPS interrupt
}

} // namespace Interfaces