export module Interfaces:AudioInterface;

import std;
import Util;

import :Interface;
import :MipsInterface;
import :AudioInterfaceTypes;

namespace Interfaces {

export class AudioInterface : public Interface {
  public:
    AudioInterface(std::shared_ptr<Util::Logger> logger,
                   MipsInterface*                mipsInterface)
        : m_logger(logger), m_mipsInterface(mipsInterface) {};

    auto read(uint32_t addr) -> uint32_t override;
    auto write(uint32_t addr, uint32_t data) -> void override;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    MipsInterface*                m_mipsInterface;

    uint32_t  m_dramAddr;
    uint32_t  m_length;
    uint8_t   m_bitrate;
    uint16_t  m_dacRate;
    AI_STATUS m_status;
    bool      m_enabled = false;
};

auto AudioInterface::read(uint32_t addr) -> uint32_t {
    contract_assert(addr % 4 == 0 &&
                    AI_REG_ADDR::BASE <= addr && addr <= AI_REG_ADDR::END);

    addr = AI_REG_ADDR::BASE + (addr & 0x1F);

    auto readReg = [this](uint32_t addr) -> uint32_t {
        switch (addr) {
            case AI_REG_ADDR::AI_DRAM_ADDR: [[fallthrough]];
            case AI_REG_ADDR::AI_DACRATE: [[fallthrough]];
            case AI_REG_ADDR::AI_BITRATE: [[fallthrough]];
            case AI_REG_ADDR::AI_CONTROL: [[fallthrough]];
            case AI_REG_ADDR::AI_LENGTH:
                return m_length;
            case AI_REG_ADDR::AI_STATUS:
                return std::bit_cast<uint32_t>(m_status);
            default:
                throw Util::Error("No AI register found for addr {:#08x}", addr);
        }
    };

    auto data = readReg(addr);

    logOperation<AI_REG_ADDR>(m_logger, "read", addr, data);

    return data;
}

auto AudioInterface::write(uint32_t addr, uint32_t data) -> void {
    contract_assert(addr % 4 == 0 &&
                    AI_REG_ADDR::BASE <= addr && addr <= AI_REG_ADDR::END);

    addr = AI_REG_ADDR::BASE + (addr & 0x1F);
    logOperation<AI_REG_ADDR>(m_logger, "write", addr, data);

    switch (addr) {
        case AI_REG_ADDR::AI_DRAM_ADDR: {
            m_dramAddr = data;
            return;
        }
        case AI_REG_ADDR::AI_LENGTH: {
            m_length = data;
            if (m_enabled) {
                m_mipsInterface->setInterrupt<^^MI_INTERRUPT::ai>(true);
            }
            return;
        }
        case AI_REG_ADDR::AI_CONTROL: {
            m_enabled = std::bit_cast<AI_CONTROL>(data).dmaEnable; // TODO do we need to initiate a pending DMA here
            return;
        }
        case AI_REG_ADDR::AI_STATUS: {
            m_mipsInterface->setInterrupt<^^MI_INTERRUPT::ai>(false);
            return;
        }
        case AI_REG_ADDR::AI_DACRATE: {
            m_dacRate = data;
            return;
        }
        case AI_REG_ADDR::AI_BITRATE: {
            m_bitrate = data;
            return;
        }
    }
}

} // namespace Interfaces