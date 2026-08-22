module;
#include <util/defines.hpp>
export module Interfaces:SerialInterface;

import std;
import Rom;
import Util;

import :Interface;
import :MipsInterface;
import :SerialInterfaceTypes;

namespace Interfaces {

export class SerialInterface : public Interface {
  public:
    SerialInterface(
        std::shared_ptr<Util::Logger> logger,
        std::byte*                    memory,
        MipsInterface*                mipsInterface)
        : m_logger(logger), m_memory(memory), m_mipsInterface(mipsInterface) {}

    auto loadPifRom(const RomFile* rom) -> void;

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

    template <std::integral T>
    auto readBus(uint32_t addr) -> T;

    template <std::integral T>
    auto writeBus(uint32_t addr, T data) -> void;

  private:
    auto dmaMemcpy(uint32_t dst, uint32_t src) -> void;

    std::shared_ptr<Util::Logger> m_logger;
    const RomFile*                m_pifRom;
    std::byte*                    m_memory;
    MipsInterface*                m_mipsInterface;
    uint32_t                      m_dramAddr;
    uint32_t                      m_pifAddr;
    SI_STATUS                     m_status;

    bool m_pifCmdPending = false;
};

auto SerialInterface::loadPifRom(const RomFile* rom) -> void {
    m_pifRom = rom;
}

auto SerialInterface::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    SI_REG_ADDR::BASE <= addr && addr <= SI_REG_ADDR::END);
    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case SI_REG_ADDR::SI_DRAM_ADDR:
                return m_dramAddr;
            case SI_REG_ADDR::SI_PIF_AD_RD64B:
                return m_pifAddr;
            case SI_REG_ADDR::SI_PIF_AD_WR4B: [[fallthrough]];
            case SI_REG_ADDR::SI_PIF_AD_WR64B: [[fallthrough]];
            case SI_REG_ADDR::SI_PIF_AD_RD4B:
                return 0;
            case SI_REG_ADDR::SI_STATUS:
                m_status.interrupt = m_mipsInterface->getInterrupt<^^MI_INTERRUPT::si>();
                return std::bit_cast<uint32_t>(m_status);
            default:
                throw Util::Error("No SI register found for addr {:#08x}", addr);
        }
    };

    auto data = readReg(addr);

    logOperation<SI_REG_ADDR>(m_logger, "read", addr, data);

    return data;
}

auto SerialInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    SI_REG_ADDR::BASE <= addr && addr <= SI_REG_ADDR::END);

    logOperation<SI_REG_ADDR>(m_logger, "write", addr, data);

    switch (addr) {
        case SI_REG_ADDR::SI_DRAM_ADDR:
            m_dramAddr = data;
            break;
        case SI_REG_ADDR::SI_PIF_AD_RD64B: {
            m_pifAddr = data;
            dmaMemcpy(m_dramAddr, m_pifAddr);
            m_mipsInterface->setInterrupt<^^MI_INTERRUPT::si>(true);
            break;
        };
        case SI_REG_ADDR::SI_PIF_AD_WR4B: [[fallthrough]];
        case SI_REG_ADDR::SI_PIF_AD_WR64B: [[fallthrough]];
        case SI_REG_ADDR::SI_PIF_AD_RD4B:
            logWarnOnIgnoredRegister<SI_REG_ADDR>(m_logger, addr);
            break;
        case SI_REG_ADDR::SI_STATUS:
            m_mipsInterface->setInterrupt<^^MI_INTERRUPT::si>(false);
            break;
    }
}

template <std::integral T>
auto SerialInterface::readBus(uint32_t addr) -> T {
    static bool f;
    const auto [e, range] = Util::getRange<SiDmaRanges>(addr);
    switch (e) {
        case SiDmaRanges::PIF_ROM:
            IF_LOG_ENABLED(m_logger) {
                if (!m_pifRom) {
                    m_logger->log<Util::Verbosity::MED>(
                        std::tuple{"sys", std::meta::display_string_of(^^SerialInterface)},
                        std::tuple{"warning", "Skipping read of missing PIF ROM"});
                }
            }
            return m_pifRom ? m_pifRom->read<T>(addr - range.lower) : 0;
        case SiDmaRanges::PIF_RAM:
            if (addr - range.lower == 0x24) {
                return static_cast<T>(0x0000913F); // TODO add other CIC values
            } else if (addr - range.lower >= 0x30 && addr - range.lower < 0x40) {
                m_pifCmdPending = !m_pifCmdPending; // workaround to simulate processing time
                return static_cast<T>(m_pifCmdPending << 7);
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", std::meta::display_string_of(^^SerialInterface)},
                    std::tuple{"warning", "Ignoring access to PIF RAM @ {:#08x}", addr});
            }
            return 0;
        default:
            throw Util::Error("SI: Read from unknown address {:#08x}", addr);
    }
    return 0;
}

template <std::integral T>
auto SerialInterface::writeBus(uint32_t addr, T data) -> void {
    const auto [e, range] = Util::getRange<SiDmaRanges>(addr);
    switch (e) {
        case SiDmaRanges::PIF_ROM:
            throw Util::Error("SI: Attempt to write to read-only memory (PIF_ROM)");
        case SiDmaRanges::PIF_RAM:
            if (addr - range.lower >= 0x30 && addr - range.lower < 0x40) {
                return;
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", std::meta::display_string_of(^^SerialInterface)},
                    std::tuple{"warning", "Ignoring write to PIF RAM @ {:#08x}", addr});
            }
            break;
        default:
            throw Util::Error("SI: Write to unknown address {:#08x}", addr);
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