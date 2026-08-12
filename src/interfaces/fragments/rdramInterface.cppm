export module Interfaces:RdramInterface;

import std;
import Util;

import :MmioRegisters;
import :RdramInterfaceTypes;

namespace Interfaces {

export class RdramInterface : public MmioRegisters {
    using RegisterPtr = std::variant<RI_MODE*, RI_CONFIG*, RI_CURRENT_LOAD*, RI_SELECT*, RI_REFRESH*, RI_LATENCY*, RI_ERROR*, RI_BANK_STATUS*>;

  public:
    RdramInterface(std::shared_ptr<Util::Logger> logger)
        : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;

    auto write(uint32_t addr, uint32_t data) -> void override;

    auto getReg(uint32_t addr) -> RegisterPtr //
        pre(RI_REG_ADDR::BASE <= addr && addr <= RI_REG_ADDR::END);

  private:
    riRegs m_regs{};

    std::shared_ptr<Util::Logger> m_logger;
};

auto RdramInterface::read(uint32_t addr) -> uint32_t {
    return getReg(addr).visit([=, this](auto&& v) {
        auto value = std::bit_cast<uint32_t>(*v);
        if (m_logger && m_logger->enabled()) {
            constexpr auto name = std::meta::display_string_of(^^decltype(*v));
            m_logger->log<Util::Verbosity::MED>(
                std::tuple{"sys", "RI"},
                std::tuple{"op", "read"},
                std::tuple{"reg", "{}", name},
                std::tuple{"data", "0x{:08x}", value});
        }
        return value;
    });
}

auto RdramInterface::write(uint32_t addr, uint32_t data) -> void {
    getReg(addr).visit([=, this](auto&& v) {
        if constexpr (WriteableRegister_c<decltype(v)>) {
            v->write(data);
            if (m_logger && m_logger->enabled()) {
                constexpr auto name = std::meta::display_string_of(^^decltype(*v));
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", "RI"},
                    std::tuple{"op", "write"},
                    std::tuple{"reg", "{}", name},
                    std::tuple{"data", "0x{:08x}", data});
            }
        }
    });
}

auto RdramInterface::getReg(uint32_t addr) -> RdramInterface::RegisterPtr {
    auto offset = addr - RI_REG_ADDR::BASE;
    switch (offset) {
        case RI_REG_ADDR::RI_MODE_OFFSET:
            return &(m_regs.riMode);
        case RI_REG_ADDR::RI_CONFIG_OFFSET:
            return &(m_regs.riConfig);
        case RI_REG_ADDR::RI_CURRENT_LOAD_OFFSET:
            return &(m_regs.riCurrentLoad);
        case RI_REG_ADDR::RI_SELECT_OFFSET:
            return &(m_regs.riSelect);
        case RI_REG_ADDR::RI_REFRESH_OFFSET:
            return &(m_regs.riRefresh);
        case RI_REG_ADDR::RI_LATENCY_OFFSET:
            return &(m_regs.riLatency);
        case RI_REG_ADDR::RI_ERROR_OFFSET:
            return &(m_regs.riError);
        case RI_REG_ADDR::RI_BANK_STATUS_OFFSET:
            return &(m_regs.riBankStatus);
        default:
            throw std::runtime_error(std::format("No RI register found for addr {:#08x}", addr));
    };
}

} // namespace Interfaces