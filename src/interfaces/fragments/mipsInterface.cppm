export module Interfaces:MipsInterface;

import std;
import Util;

import :MmioRegisters;
import :MipsInterfaceTypes;

namespace Interfaces {

export class MipsInterface : public MmioRegisters {
    using RegisterPtr = std::variant<MI_MODE*, MI_VERSION*, MI_INTERRUPT*, MI_MASK*>;

  public:
    MipsInterface(std::shared_ptr<Util::Logger> logger)
        : m_logger(logger) {};

    auto read(uint32_t addr) -> uint32_t override;

    auto write(uint32_t addr, uint32_t data) -> void override;

    auto getReg(uint32_t addr) -> RegisterPtr //
        pre(MI_REG_ADDR::BASE <= addr && addr <= MI_REG_ADDR::END);

  private:
    miRegs m_regs{};

    std::shared_ptr<Util::Logger> m_logger;
};

auto MipsInterface::read(uint32_t addr)
    -> uint32_t {
    return getReg(addr).visit([=, this](auto&& v) {
        auto value = std::bit_cast<uint32_t>(*v);
        if (m_logger && m_logger->enabled()) {
            m_logger->log<Util::Verbosity::MED>(
                std::tuple{"sys", "MI"},
                std::tuple{"op", "read"},
                std::tuple{"reg", "{}", std::meta::display_string_of(^^decltype(v))},
                std::tuple{"data", "0x{:08x}", value});
        }
        return value;
    });
}

auto MipsInterface::write(uint32_t addr, uint32_t data) -> void {
    getReg(addr).visit([=, this](auto&& v) {
        if constexpr (WriteableRegister_c<decltype(v)>) {
            if constexpr (std::is_same_v<MI_MODE*, std::remove_reference_t<decltype(v)>>) {
                auto wr = std::bit_cast<MI_MODE::MI_MODE_write>(data);
                if (wr.clearDp) {
                    m_regs.miInterrupt.dp = 0;
                }
            }

            v->write(data);
            if (m_logger && m_logger->enabled()) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"sys", "MI"},
                    std::tuple{"op", "write"},
                    std::tuple{"reg", "{}", std::meta::display_string_of(^^decltype(v))},
                    std::tuple{"data", "0x{:08x}", data});
            }
        }
    });
}

auto MipsInterface::getReg(uint32_t addr) -> MipsInterface::RegisterPtr {
    auto offset = addr & 0xF;
    switch (offset) {
        case MI_REG_ADDR::MI_MODE_OFFSET:
            return &(m_regs.miMode);
        case MI_REG_ADDR::MI_VERSION_OFFSET:
            return &(m_regs.miVersion);
        case MI_REG_ADDR::MI_INTERRUPT_OFFSET:
            return &(m_regs.miInterrupt);
        case MI_REG_ADDR::MI_MASK_OFFSET:
            return &(m_regs.miMask);
        default:
            throw std::runtime_error(std::format("No MI register found for addr {:#08x}", addr));
    };
}

} // namespace Interfaces