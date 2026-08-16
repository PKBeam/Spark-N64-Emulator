export module Interfaces:RdramInterface;

import std;
import Util;

import :MmioRegisters;
import :RdramInterfaceTypes;

namespace Interfaces {

export class RdramInterface : public MmioRegisters {
  public:
    RdramInterface(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;

    RI_MODE        m_mode{};
    RI_CONFIG      m_config{};
    RI_SELECT      m_select{};
    RI_REFRESH     m_refresh{};
    RI_LATENCY     m_latency{};
    RI_ERROR       m_error{};
    RI_BANK_STATUS m_bankStatus{};
};

auto RdramInterface::read(uint32_t addr) -> uint32_t {
    contract_assert(RI_REG_ADDR::BASE <= addr && addr <= RI_REG_ADDR::END);
    addr = RI_REG_ADDR::BASE + (addr & 0xFF);

    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case RI_REG_ADDR::RI_MODE: return std::bit_cast<uint32_t>(m_mode);
            case RI_REG_ADDR::RI_CONFIG: return std::bit_cast<uint32_t>(m_config);
            case RI_REG_ADDR::RI_CURRENT_LOAD: return std::bit_cast<uint32_t>(RI_CURRENT_LOAD{});
            case RI_REG_ADDR::RI_SELECT: return std::bit_cast<uint32_t>(m_select);
            case RI_REG_ADDR::RI_REFRESH: return std::bit_cast<uint32_t>(m_refresh);
            case RI_REG_ADDR::RI_LATENCY: return std::bit_cast<uint32_t>(m_latency);
            case RI_REG_ADDR::RI_ERROR: return std::bit_cast<uint32_t>(m_error);
            case RI_REG_ADDR::RI_BANK_STATUS: return std::bit_cast<uint32_t>(m_bankStatus);
            default:
                throw std::runtime_error(std::format("No RI register found for addr {:#08x}", addr));
        }
    };

    auto value = readReg(addr);
    if (m_logger && m_logger->enabled()) {
        auto name = Util::enumName(static_cast<decltype(RI_REG_ADDR::RI_MODE)>(addr)).value_or("Unknown");
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", "RI"},
            std::tuple{"op", "read"},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", "0x{:08x}", value});
    }
    return value;
}

auto RdramInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(RI_REG_ADDR::BASE <= addr && addr <= RI_REG_ADDR::END);
    addr = RI_REG_ADDR::BASE + (addr & 0xFF);

    if (m_logger && m_logger->enabled()) {
        auto name = Util::enumName(static_cast<decltype(RI_REG_ADDR::RI_MODE)>(addr)).value_or("Unknown");
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", "RI"},
            std::tuple{"op", "write"},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", "0x{:08x}", data});
    }

    contract_assert(RI_REG_ADDR::BASE <= addr && addr <= RI_REG_ADDR::END);
    switch (addr) {
        case RI_REG_ADDR::RI_MODE: m_mode = std::bit_cast<RI_MODE>(data); break;
        case RI_REG_ADDR::RI_CONFIG: m_config = std::bit_cast<RI_CONFIG>(data); break;
        case RI_REG_ADDR::RI_CURRENT_LOAD: {
            // unused-ish?
            break;
        }
        case RI_REG_ADDR::RI_SELECT: m_select = std::bit_cast<RI_SELECT>(data); break;
        case RI_REG_ADDR::RI_REFRESH: m_refresh = std::bit_cast<RI_REFRESH>(data); break;
        case RI_REG_ADDR::RI_LATENCY: m_latency = std::bit_cast<RI_LATENCY>(data); break;
        case RI_REG_ADDR::RI_ERROR: {
            m_error.ack  = 0;
            m_error.nack = 0;
            m_error.over = 0;
            break;
        }
        case RI_REG_ADDR::RI_BANK_STATUS: {
            m_bankStatus.bankDirtyBits = 0xFF;
            m_bankStatus.bankValidBits = 0;
            break;
        }
        default:
            throw std::runtime_error(std::format("No RI register found for addr {:#08x}", addr));
    }
}

} // namespace Interfaces