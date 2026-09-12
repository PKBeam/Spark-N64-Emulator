module;

#include <util/defines.hpp>
#include <rdp_gfx_backend/gfxBackend.hpp>

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
        Interfaces::MipsInterface*    mipsInterface,
        GfxBackend*                   gfxBackend)
        : m_logger(logger),
          m_rdpControl(rdpControl),
          m_mipsInterface(mipsInterface),
          m_gfxBackend(gfxBackend) {};

    auto runRdpCommand() -> void;

    constexpr auto setTerminateAfterSyncs(int terminateAfterSyncs) -> void {
        m_terminateAfterSyncs = terminateAfterSyncs;
    }

    constexpr auto getSyncCount() const -> std::size_t {
        return m_syncs;
    }

    constexpr auto registerSyncCallback(std::size_t syncCount, std::function<void()> callback) -> void {
        m_syncCallbackCount = syncCount;
        m_syncCallback      = std::move(callback);
    }

  private:
    std::shared_ptr<Util::Logger> m_logger;
    ::RDP::Control*               m_rdpControl{};
    Interfaces::MipsInterface*    m_mipsInterface{};
    std::size_t                   m_syncs{};
    std::optional<Util::Colour>   m_primColour{};

    GfxBackend*           m_gfxBackend{};
    int                   m_terminateAfterSyncs{-1};
    std::function<void()> m_syncCallback{};
    std::size_t           m_syncCallbackCount{};
};

template <typename CommandT, std::size_t NumWords>
    requires(sizeof(CommandT) == NumWords * sizeof(uint64_t))
auto makeCommand(std::deque<uint64_t>& cmds) -> CommandT {
    auto cmdWords = std::array<uint64_t, NumWords>{};
    for (auto i = 0uz; i < NumWords; ++i) {
        cmdWords[i] = cmds.front();
        cmds.pop_front();
    }
    return std::bit_cast<CommandT>(cmdWords);
}

auto RDP::runRdpCommand() -> void {
    auto& cmds = m_rdpControl->getCommands();
    if (cmds.empty()) {
        return;
    }
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::LOW, Sev::INFO, Sys::RDP>("Received {} commands", cmds.size());
    }
    while (!cmds.empty()) {
        const auto cmdBits = cmds.front();
        const auto cmdType = getCommand(cmdBits);
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::MED, Sys::RDP>(
                std::tuple{"command", "{}", cmdType});
        }
        switch (cmdType) {
            case Command::SYNC_FULL: {
                m_gfxBackend->startRenderPass();
                m_gfxBackend->completeRenderFrame();
                m_mipsInterface->setInterrupt<^^Interfaces::MI_INTERRUPT::dp>(true);
                m_syncs++;
                cmds.pop_front();
                if (m_syncCallback && m_syncs == m_syncCallbackCount) {
                    m_syncCallback();
                }
                if (static_cast<int>(m_syncs) == m_terminateAfterSyncs) {
                    std::println("Reached {} syncs, terminating", m_syncs);
                    std::terminate();
                }
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::HIGH, Sev::INFO, Sys::RDP>("SYNC_FULL at sync {}", m_syncs);
                }
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

                const auto cmd = makeCommand<Commands::FillTriangle, 4>(cmds);

                auto shadeCmd = std::optional<Commands::FillTriangle::Shade>{};
                if (cmdHeader.shade) {
                    shadeCmd.emplace(makeCommand<Commands::FillTriangle::Shade, 8>(cmds));
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
                const auto tri = cmd.getRenderTriangle();
                m_gfxBackend->addTriangle(tri.data());
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "draw"},
                        std::tuple{"type", "triangle"},
                        std::tuple{"coords", "{}", cmd.getTriangle()});
                }
                break;
            }
            case Command::FILL_RECTANGLE: {
                const auto cmd  = makeCommand<Commands::FillRectangle, 1>(cmds);
                const auto tris = cmd.getRenderTriangles(); // todo fill optimisation in vk
                // m_gfxBackend->addTriangle(tris[0].data());
                // m_gfxBackend->addTriangle(tris[1].data());
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "draw"},
                        std::tuple{"type", "rectangle"},
                        std::tuple{"coords", "{}", cmd.getRectangle()});
                }
                break;
            }
            case Command::TEXTURE_RECTANGLE: {
                const auto cmd  = makeCommand<Commands::TextureRectangle, 2>(cmds);
                const auto tris = cmd.getRenderTriangles();
                // m_gfxBackend->addTriangle(tris[0].data());
                // m_gfxBackend->addTriangle(tris[1].data());
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "draw"},
                        std::tuple{"type", "rectangle"},
                        std::tuple{"coords", "{}", cmd.getRectangle()});
                }
                break;
            }
            case Command::SET_PRIMITIVE_COLOR: {
                const auto cmd = makeCommand<Commands::SetPrimitiveColor, 1>(cmds);
                m_gfxBackend->setPrimitiveColour(cmd.red, cmd.green, cmd.blue, cmd.alpha);
                m_primColour.emplace(cmd.red / 255.0f, cmd.green / 255.0f, cmd.blue / 255.0f, cmd.alpha / 255.0f);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setColour"},
                        std::tuple{"colour", "{}", *m_primColour});
                }
                break;
            }
            case Command::SYNC_PIPE:
                m_gfxBackend->startRenderPass();
                cmds.pop_front();
                break;
            // ignore for now
            case Command::SET_COMBINE_MODE: [[fallthrough]];
            case Command::SET_OTHER_MODES: [[fallthrough]];
            case Command::SET_BLEND_COLOR: [[fallthrough]];
            case Command::SET_FOG_COLOR: [[fallthrough]];
            case Command::SET_COLOR_IMAGE: [[fallthrough]];
            case Command::SET_DEPTH_IMAGE: [[fallthrough]];
            case Command::SET_TEXTURE_IMAGE: [[fallthrough]];
            case Command::SET_FILL_COLOR: [[fallthrough]];
            case Command::LOAD_BLOCK: [[fallthrough]];
            case Command::SET_TILE: [[fallthrough]];
            case Command::SET_TILE_SIZE: [[fallthrough]];
            case Command::SET_ENVIRONMENT_COLOR: [[fallthrough]];
            case Command::LOAD_TLUT: [[fallthrough]];
            case Command::LOAD_TILE: [[fallthrough]];
            case Command::SET_SCISSOR:
                cmds.pop_front();
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sev::WARNING, Sys::RDP>("Ignoring command {}", cmdType);
                }
                break;
            // No-ops
            case Command::SYNC_LOAD: [[fallthrough]];
            case Command::SYNC_TILE:
                cmds.pop_front();
                break;
            default:
                throw Util::Error("RDP unimplemented command {}", cmdType);
        }
    }
}
} // namespace RDP
