module;

#include <util/defines.hpp>

export module RDP:RDP;

import std;
import Interfaces;
import InterfaceTypes;
import ISA;
import RdpControl;
import Util;

import :Commands;

export namespace RDP {

class RDP {
  public:
    RDP(std::shared_ptr<Util::Logger> logger,
        ::RDP::Control*               rdpControl,
        Interfaces::MipsInterface*    mipsInterface)
        : m_logger(logger),
          m_rdpControl(rdpControl),
          m_mipsInterface(mipsInterface) {};

    auto runCommand() -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    ::RDP::Control*               m_rdpControl{};
    Interfaces::MipsInterface*    m_mipsInterface{};
};

template <typename CommandT, std::size_t NumWords>
    requires(sizeof(CommandT) == NumWords * sizeof(uint64_t))
auto makeCommand(std::deque<uint64_t>& cmds, uint64_t firstWord) -> CommandT {
    auto cmdWords = std::array<uint64_t, NumWords>{};
    cmdWords[0]   = firstWord;
    for (auto i = 1uz; i < NumWords; ++i) {
        cmdWords[i] = cmds.front();
        cmds.pop_front();
    }
    return std::bit_cast<CommandT>(cmdWords);
}

auto RDP::runCommand() -> void {
    auto cmds = m_rdpControl->getCommands();
    if (cmds.empty()) {
        return;
    }
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sev::INFO, Sys::RDP>("Received {} commands", cmds.size());
    }
    while (!cmds.empty()) {
        const auto cmdBits = cmds.front();
        cmds.pop_front();
        const auto cmdType = getCommand(cmdBits);
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::HIGH, Sys::RDP>(
                std::tuple{"command", "{}", cmdType});
        }
        switch (cmdType) {
            case Command::SYNC_FULL: {
                m_mipsInterface->setInterrupt<^^Interfaces::MI_INTERRUPT::dp>(true);
                break;
            }
            case Command::FILL_TRIANGLE: [[fallthrough]];
            case Command::FILL_TRIANGLE_Z: [[fallthrough]];
            case Command::FILL_TRIANGLE_T: [[fallthrough]];
            case Command::FILL_TRIANGLE_TZ: [[fallthrough]];
            case Command::FILL_TRIANGLE_S: [[fallthrough]];
            case Command::FILL_TRIANGLE_SZ: [[fallthrough]];
            case Command::FILL_TRIANGLE_ST: [[fallthrough]];
            case Command::FILL_TRIANGLE_STZ: {
                const auto cmdHeader = std::bit_cast<Commands::FillTriangle::Cmd>(static_cast<uint8_t>(cmdType));

                const auto cmd = makeCommand<Commands::FillTriangle, 4>(cmds, cmdBits);

                if (cmdHeader.shade) {
                    for (auto _ : std::views::iota(0, 8)) {
                        cmds.pop_front();
                    }
                }
                if (cmdHeader.texture) {
                    for (auto _ : std::views::iota(0, 8)) {
                        cmds.pop_front();
                    }
                }
                if (cmdHeader.zbuffer) {
                    for (auto _ : std::views::iota(0, 2)) {
                        cmds.pop_front();
                    }
                }
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::HIGH, Sys::RDP>(
                        std::tuple{"op", "draw"},
                        std::tuple{"type", "triangle"},
                        std::tuple{"coords", "{}", cmd.getTriangle()});
                }
                break;
            }
            case Command::FILL_RECTANGLE: {
                const auto cmd = makeCommand<Commands::FillRectangle, 1>(cmds, cmdBits);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::HIGH, Sys::RDP>(
                        std::tuple{"op", "draw"},
                        std::tuple{"type", "rectangle"},
                        std::tuple{"coords", "{}", cmd.getRectangle()});
                }
                break;
            }
            case Command::TEXTURE_RECTANGLE: {
                const auto cmd = makeCommand<Commands::TextureRectangle, 2>(cmds, cmdBits);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::HIGH, Sys::RDP>(
                        std::tuple{"op", "draw"},
                        std::tuple{"type", "rectangle"},
                        std::tuple{"coords", "{}", cmd.getRectangle()});
                }
                break;
            }
            // ignore for now
            case Command::SET_COMBINE_MODE: [[fallthrough]];
            case Command::SET_OTHER_MODES: [[fallthrough]];
            case Command::SET_BLEND_COLOR: [[fallthrough]];
            case Command::SET_COLOR_IMAGE: [[fallthrough]];
            case Command::SET_DEPTH_IMAGE: [[fallthrough]];
            case Command::SET_TEXTURE_IMAGE: [[fallthrough]];
            case Command::SET_FILL_COLOR: [[fallthrough]];
            case Command::LOAD_BLOCK: [[fallthrough]];
            case Command::SET_TILE: [[fallthrough]];
            case Command::SET_TILE_SIZE: [[fallthrough]];
            case Command::SET_PRIMITIVE_COLOR: [[fallthrough]];
            case Command::SET_ENVIRONMENT_COLOR: [[fallthrough]];
            case Command::SET_SCISSOR:
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::HIGH, Sev::WARNING, Sys::RDP>("Ignoring command {}", cmdType);
                }
                break;
            // No-ops
            case Command::SYNC_LOAD: [[fallthrough]];
            case Command::SYNC_PIPE: [[fallthrough]];
            case Command::SYNC_TILE:
                break;
            default:
                throw Util::Error("RDP unimplemented command {}", cmdType);
        }
    }
}
} // namespace RDP
