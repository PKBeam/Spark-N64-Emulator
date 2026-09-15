module;
#include <util/defines.hpp>
#include <rdp_gfx_backend/gfxBackend.hpp>
export module RDP:RDP;

import std;
import Interfaces;
import InterfaceTypes;
import ISA;
import Memory;
import RdpControl;
import Util;

import :Commands;

struct RGBA32 {
    uint8_t alpha;
    uint8_t blue;
    uint8_t green;
    uint8_t red;
};

struct Colours {
    uint32_t fill;
    uint32_t fog;
    uint32_t blend;
    uint32_t prim;
    uint32_t environ;
};

auto logTriangle(const std::array<int32_t, 9>& triangle) -> void {
    // Implement logging logic here
}

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

export namespace RDP {

class RDP {
  public:
    constexpr static auto TMEM_SIZE = 4 * 1024; // 4 KB
    RDP(std::shared_ptr<Util::Logger> logger,
        ::RDP::Control*               rdpControl,
        Interfaces::MipsInterface*    mipsInterface,
        Memory::MemoryBus*            memoryBus,
        GfxBackend*                   gfxBackend)
        : m_logger(logger),
          m_rdpControl(rdpControl),
          m_mipsInterface(mipsInterface),
          m_memoryBus(memoryBus),
          m_gfxBackend(gfxBackend),
          m_textureMemory(reinterpret_cast<std::byte*>(std::malloc(TMEM_SIZE))) {};

    ~RDP() {
        std::free(reinterpret_cast<void*>(m_textureMemory));
    }

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
    auto makeCombinerInputs() -> ::RDP::CombineInputs;
    auto flushBackend() -> void;

    std::shared_ptr<Util::Logger> m_logger;
    ::RDP::Control*               m_rdpControl{};
    Interfaces::MipsInterface*    m_mipsInterface{};
    Memory::MemoryBus*            m_memoryBus{};
    GfxBackend*                   m_gfxBackend{};
    std::byte*                    m_textureMemory{};

    TextureImage        m_textureImage{};
    std::array<Tile, 8> m_tiles{};

    std::size_t           m_syncs{};
    int                   m_terminateAfterSyncs{-1};
    std::function<void()> m_syncCallback{};
    std::size_t           m_syncCallbackCount{};

    std::bitset<8> m_tileUsedThisDraw{};

    Commands::SetCombineMode         m_combineMode{};
    Commands::SetCombineMode::Inputs m_combineInputs{};
    Commands::SetOtherModes          m_mode{};
    uint32_t                         m_blend{};
    uint32_t                         m_fog{};
    uint32_t                         m_fill{};
};

auto RDP::makeCombinerInputs() -> ::RDP::CombineInputs {
    using Combine         = Commands::SetCombineMode;
    const bool is1Cycle   = m_mode.cycleType == Commands::SetOtherModes::CYCLE_1;
    const auto isCombined = [is1Cycle](auto input) {
        return !is1Cycle && input == Combine::Inputs::Combined;
    };
    const auto rgbA0   = m_combineInputs.inputFor(static_cast<Combine::RgbA>(m_combineMode.rgbA0));
    const auto rgbB0   = m_combineInputs.inputFor(static_cast<Combine::RgbB>(m_combineMode.rgbB0));
    const auto rgbC0   = m_combineInputs.inputFor(static_cast<Combine::RgbC>(m_combineMode.rgbC0));
    const auto rgbD0   = m_combineInputs.inputFor(static_cast<Combine::RgbD>(m_combineMode.rgbD0));
    const auto rgbA1   = m_combineInputs.inputFor(static_cast<Combine::RgbA>(m_combineMode.rgbA1));
    const auto rgbB1   = m_combineInputs.inputFor(static_cast<Combine::RgbB>(m_combineMode.rgbB1));
    const auto rgbC1   = m_combineInputs.inputFor(static_cast<Combine::RgbC>(m_combineMode.rgbC1));
    const auto rgbD1   = m_combineInputs.inputFor(static_cast<Combine::RgbD>(m_combineMode.rgbD1));
    const auto alphaA0 = m_combineInputs.inputFor(static_cast<Combine::AlphaA>(m_combineMode.alphaA0));
    const auto alphaB0 = m_combineInputs.inputFor(static_cast<Combine::AlphaB>(m_combineMode.alphaB0));
    const auto alphaC0 = m_combineInputs.inputFor(static_cast<Combine::AlphaC>(m_combineMode.alphaC0));
    const auto alphaD0 = m_combineInputs.inputFor(static_cast<Combine::AlphaD>(m_combineMode.alphaD0));
    const auto alphaA1 = m_combineInputs.inputFor(static_cast<Combine::AlphaA>(m_combineMode.alphaA1));
    const auto alphaB1 = m_combineInputs.inputFor(static_cast<Combine::AlphaB>(m_combineMode.alphaB1));
    const auto alphaC1 = m_combineInputs.inputFor(static_cast<Combine::AlphaC>(m_combineMode.alphaC1));
    const auto alphaD1 = m_combineInputs.inputFor(static_cast<Combine::AlphaD>(m_combineMode.alphaD1));
    const auto inputs  = ::RDP::CombineInputs{
         .rgba0 = {
             .a = is1Cycle ? 0 : static_cast<int32_t>(rgbA0 | alphaA0),
             .b = is1Cycle ? 0 : static_cast<int32_t>(rgbB0 | alphaB0),
             .c = is1Cycle ? 0 : static_cast<int32_t>(rgbC0 | alphaC0),
             .d = is1Cycle ? 0 : static_cast<int32_t>(rgbD0 | alphaD0),
        },
         .rgba1 = {
             .a = static_cast<int32_t>(rgbA1 | alphaA1),
             .b = static_cast<int32_t>(rgbB1 | alphaB1),
             .c = static_cast<int32_t>(rgbC1 | alphaC1),
             .d = static_cast<int32_t>(rgbD1 | alphaD1),
        },
         .usePreviousRgb = {
             .a = isCombined(rgbA1),
             .b = isCombined(rgbB1),
             .c = isCombined(rgbC1),
             .d = isCombined(rgbD1),
        },
         .usePreviousAlpha = {
             .a = isCombined(alphaA1),
             .b = isCombined(alphaB1),
             .c = isCombined(alphaC1),
             .d = isCombined(alphaD1),
        }};
    return inputs;
}

auto RDP::flushBackend() -> void {
    const auto inputs = makeCombinerInputs();
    m_gfxBackend->setCombineInputs(inputs);
    for (auto tileIndex = 0uz; tileIndex < m_tileUsedThisDraw.size(); ++tileIndex) {
        if (!m_tileUsedThisDraw[tileIndex]) {
            continue;
        }

        const auto& tile = m_tiles[tileIndex];
        m_gfxBackend->updateTile(
            tileIndex,
            m_textureMemory + tile.tmemAddress,
            tile.params());
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::MED, Sys::RDP>(
                std::tuple{"op", "flushTile"},
                std::tuple{"tile", "{}", tileIndex},
                std::tuple{"tmemAddress", "{}", tile.tmemAddress},
                std::tuple{"width", "{}", tile.extent.width()},
                std::tuple{"height", "{}", tile.extent.height()},
                std::tuple{"format", "{}", tile.format});
        }
    }
    m_gfxBackend->startRenderPass();
    m_tileUsedThisDraw.reset();
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
            m_logger->log<Level::HIGH, Sys::RDP>(
                std::tuple{"command", "{}", cmdType});
        }
        switch (cmdType) {
            case Command::SYNC_PIPE: {
                flushBackend();
                cmds.pop_front();
                break;
            }
            case Command::SYNC_FULL: {
                flushBackend();
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
                const auto tri = cmd.getTriangle();
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "drawTriangle"},
                        std::tuple{"tile", "{}", cmd.tile},
                        std::tuple{"coords", "{}", tri});
                }
                auto shade = Util::RenderTriangle{};
                if (cmdHeader.shade) {
                    const auto shadeCmd = makeCommand<Commands::FillTriangle::Shade, 8>(cmds);
                }

                auto texCoords = Util::RenderTriangle{};
                if (cmdHeader.texture) {
                    const auto textureCmd = makeCommand<Commands::FillTriangle::Texture, 8>(cmds);
                    texCoords             = textureCmd.getTexCoords(tri, m_mode.perspTexEn);
                    IF_LOG_ENABLED(m_logger) {
                        m_logger->log<Level::MED, Sys::RDP>(
                            std::tuple{"op", "triangleTexture"},
                            std::tuple{"coords", "{}", texCoords});
                    }
                }
                if (cmdHeader.zbuffer) {
                    for (auto _ : std::views::iota(0, 2)) {
                        cmds.pop_front();
                    }
                }
                m_tileUsedThisDraw.set(cmd.tile);
                m_gfxBackend->addTriangle(cmd.tile, tri.bytes(), shade.bytes(), texCoords.bytes());
                break;
            }
            case Command::FILL_RECTANGLE: {
                const auto cmd  = makeCommand<Commands::FillRectangle, 1>(cmds);
                const auto tris = cmd.getTriangles(); // todo fill optimisation in vk
                // m_gfxBackend->addTriangle(tris[0].bytes(), nullptr, nullptr, 0);
                // m_gfxBackend->addTriangle(tris[1].bytes(), nullptr, nullptr, 0);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "drawRectangle"},
                        std::tuple{"coords", "{}", cmd.getRectangle()});
                }
                break;
            }
            case Command::TEXTURE_RECTANGLE: {
                const auto cmd           = makeCommand<Commands::TextureRectangle, 2>(cmds);
                const auto [coords, uvs] = cmd.getTriangles();
                m_tileUsedThisDraw.set(cmd.tile);
                m_gfxBackend->addTriangle(cmd.tile, coords[0].bytes(), nullptr, uvs[0].bytes());
                m_gfxBackend->addTriangle(cmd.tile, coords[1].bytes(), nullptr, uvs[1].bytes());
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "drawRectangle"},
                        std::tuple{"tile", "{}", cmd.tile},
                        std::tuple{"coords", "{}, {}", coords[0], coords[1]},
                        std::tuple{"texture", "{}, {}", uvs[0], uvs[1]});
                }
                break;
            }
            case Command::SET_OTHER_MODES: {
                m_mode = makeCommand<Commands::SetOtherModes, 1>(cmds);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setOtherModes"},
                        std::tuple{"modes", HEXFMT64, std::bit_cast<uint64_t>(m_mode)});
                }
                break;
            }
            case Command::SET_PRIMITIVE_COLOR: {
                const auto cmd            = makeCommand<Commands::SetPrimitiveColor, 1>(cmds);
                m_combineInputs.primitive = std::bit_cast<uint32_t>(RGBA32{cmd.red, cmd.green, cmd.blue, cmd.alpha});
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setPrim"},
                        std::tuple{"colour", HEXFMT32, m_combineInputs.primitive});
                }
                break;
            }
            case Command::SET_BLEND_COLOR: {
                const auto cmd = makeCommand<Commands::SetBlendColor, 1>(cmds);
                m_blend        = std::bit_cast<uint32_t>(RGBA32{cmd.red, cmd.green, cmd.blue, cmd.alpha});
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setBlend"},
                        std::tuple{"colour", HEXFMT32, m_blend});
                }
                break;
            }
            case Command::SET_FOG_COLOR: {
                const auto cmd = makeCommand<Commands::SetFogColor, 1>(cmds);
                m_fog          = std::bit_cast<uint32_t>(RGBA32{cmd.red, cmd.green, cmd.blue, cmd.alpha});
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setFogColor"},
                        std::tuple{"colour", HEXFMT32, m_fog});
                }
                break;
            }
            case Command::SET_FILL_COLOR: {
                const auto cmd = makeCommand<Commands::SetFillColor, 1>(cmds);
                m_fill         = cmd.color;
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setFillColor"},
                        std::tuple{"colour", HEXFMT32, m_fill});
                }
                break;
            }
            case Command::SET_ENVIRONMENT_COLOR: {
                const auto cmd              = makeCommand<Commands::SetEnvironmentColor, 1>(cmds);
                m_combineInputs.environment = std::bit_cast<uint32_t>(RGBA32{cmd.red, cmd.green, cmd.blue, cmd.alpha});
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setEnvironmentColor"},
                        std::tuple{"colour", HEXFMT32, m_combineInputs.environment});
                }
                break;
            }
            case Command::SET_COMBINE_MODE: {
                m_combineMode = makeCommand<Commands::SetCombineMode, 1>(cmds);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setCombineMode"},
                        std::tuple{"combineMode", HEXFMT64, std::bit_cast<uint64_t>(m_combineMode)});
                }
                break;
            }
            case Command::SET_TILE: {
                const auto cmd = makeCommand<Commands::SetTile, 1>(cmds);

                const auto tmemAddr = cmd.address * 8;
                m_tiles[cmd.index]  = Tile{
                     .format      = {cmd.format, cmd.size},
                     .lineLength  = cmd.line,
                     .tmemAddress = static_cast<uint16_t>(tmemAddr),
                     .extent      = {Util::Fxp_0, Util::Fxp_0, Util::Fxp_0, Util::Fxp_0},
                     .s =
                         {
                             .shift  = static_cast<int8_t>(cmd.shiftS > 10 ? 16 - cmd.shiftS : -static_cast<int>(cmd.shiftS)),
                             .mirror = static_cast<uint8_t>(cmd.mirrorS),
                             .clamp  = static_cast<uint8_t>(cmd.clampS),
                             .mask   = cmd.maskS,
                        },
                     .t =
                         {
                             .shift  = static_cast<int8_t>(cmd.shiftT > 10 ? 16 - cmd.shiftT : -static_cast<int>(cmd.shiftT)),
                             .mirror = static_cast<uint8_t>(cmd.mirrorT),
                             .clamp  = static_cast<uint8_t>(cmd.clampT),
                             .mask   = cmd.maskT,
                        },
                };
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setTile"},
                        std::tuple{"tile", "{}", cmd.index},
                        std::tuple{"address", HEXFMT12, tmemAddr},
                        std::tuple{"lineLength", HEXFMT12, cmd.line},
                        std::tuple{"pixelFormat", "{}", m_tiles[cmd.index].format},
                        std::tuple{"data", HEXFMT64, std::bit_cast<uint64_t>(cmd)});
                }
                break;
            }
            case Command::SET_TILE_SIZE: {
                const auto cmd    = makeCommand<Commands::SetTileSize, 1>(cmds);
                const auto extent = Tile::Extent{
                    .ulS = Util::UFixedPoint<10, 2>::fromBits(cmd.upperLeftS),
                    .ulT = Util::UFixedPoint<10, 2>::fromBits(cmd.upperLeftT),
                    .lrS = Util::UFixedPoint<10, 2>::fromBits(cmd.lowerRightS),
                    .lrT = Util::UFixedPoint<10, 2>::fromBits(cmd.lowerRightT),
                };
                m_tiles[cmd.index].extent = extent;
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setTileSize"},
                        std::tuple{"tile", "{}", cmd.index},
                        std::tuple{"extent", "{}", extent});
                }
                break;
            }
            case Command::SET_TEXTURE_IMAGE: {
                const auto cmd = makeCommand<Commands::SetTextureImage, 1>(cmds);
                m_textureImage = {
                    .pixelSize  = cmd.size,
                    .imageWidth = cmd.width,
                    .rdramAddr  = cmd.dramAddress,
                };
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setTextureImage"},
                        std::tuple{"addr", HEXFMT32, cmd.dramAddress},
                        std::tuple{"pixelSize", "{}", cmd.size});
                }
                break;
            }
            case Command::LOAD_BLOCK: {
                const auto cmd = makeCommand<Commands::LoadBlock, 1>(cmds);
                const auto ulS = cmd.upperLeftS;
                const auto ulT = cmd.upperLeftT;
                const auto lrS = cmd.lowerRightS + 1;

                const auto tmemBaseAddr  = m_tiles[cmd.tile].tmemAddress;
                const auto texelSize     = m_textureImage.pixelSize.bytesPerPixel();
                const auto texelOffset   = ulT * m_textureImage.imageWidth + ulS;
                const auto rdramBaseAddr = m_textureImage.rdramAddr + texelOffset * texelSize;

                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "loadBlock"},
                        std::tuple{"tile", "{}", cmd.tile},
                        std::tuple{"rdramAddr", HEXFMT32, rdramBaseAddr},
                        std::tuple{"coords", "(" HEXFMT12 ", " HEXFMT12 "), (" HEXFMT12 ")", ulS, ulT, lrS});
                }

                const auto loadSize = (lrS - ulS) * texelSize;
                for (const auto word : std::views::iota(0uz, loadSize / 8)) {
                    const auto offset   = word * 8;
                    const auto value    = Util::byteswapIfLittleEndian(m_memoryBus->readPhysical<uint64_t>(rdramBaseAddr + offset));
                    const auto tmemAddr = (tmemBaseAddr + offset) & 0xFFF;
                    std::memcpy(m_textureMemory + tmemAddr, &value, sizeof(value));
                    IF_LOG_ENABLED(m_logger) {
                        m_logger->log<Level::LOW, Sys::RDP>(
                            std::tuple{"op", "w"},
                            std::tuple{"addr", HEXFMT12, tmemAddr},
                            std::tuple{"value", HEXFMT64, value});
                    }
                }
                m_tiles[cmd.tile].extent = {Util::UFixedPoint<10, 2>::fromBits(ulS),
                                            Util::UFixedPoint<10, 2>::fromBits(ulT),
                                            Util::UFixedPoint<10, 2>::fromBits(lrS),
                                            Util::UFixedPoint<10, 2>::fromBits(cmd.dxt)};
                break;
            }
            case Command::LOAD_TILE: {
                const auto cmd = makeCommand<Commands::LoadTile, 1>(cmds);
                const auto ulS = cmd.upperLeftS >> 2;
                const auto ulT = cmd.upperLeftT >> 2;
                const auto lrS = (cmd.lowerRightS >> 2) + 1;
                const auto lrT = (cmd.lowerRightT >> 2) + 1;

                const auto width  = lrS - ulS;
                const auto height = lrT - ulT;

                const auto tmemBaseAddr  = m_tiles[cmd.tile].tmemAddress;
                const auto texelSize     = m_textureImage.pixelSize.bytesPerPixel();
                const auto texelOffset   = ulT * m_textureImage.imageWidth + ulS;
                const auto rdramBaseAddr = m_textureImage.rdramAddr + texelOffset * texelSize;

                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "loadTile"},
                        std::tuple{"tile", "{}", cmd.tile},
                        std::tuple{"rdramAddr", HEXFMT32, rdramBaseAddr},
                        std::tuple{"coords", "(" HEXFMT12 ", " HEXFMT12 "), (" HEXFMT12 ", " HEXFMT12 ")", ulS, ulT, lrS, lrT});
                }

                for (const auto row : std::views::iota(0, height)) {
                    for (const auto word : std::views::iota(0u, m_tiles[cmd.tile].lineLength)) {
                        auto       offset   = row * word * sizeof(uint64_t);
                        const auto value    = Util::byteswapIfLittleEndian(m_memoryBus->readPhysical<uint64_t>(rdramBaseAddr + offset));
                        const auto tmemAddr = (tmemBaseAddr + offset) & 0xFFF;
                        std::memcpy(m_textureMemory + tmemAddr, &value, sizeof(value));
                        IF_LOG_ENABLED(m_logger) {
                            m_logger->log<Level::LOW, Sys::RDP>(
                                std::tuple{"op", "w"},
                                std::tuple{"addr", HEXFMT12, tmemAddr},
                                std::tuple{"value", HEXFMT64, value});
                        }
                        offset += sizeof(uint64_t);
                    }
                }
                m_tiles[cmd.tile].extent = {Util::UFixedPoint<10, 2>::fromBits(ulS),
                                            Util::UFixedPoint<10, 2>::fromBits(ulT),
                                            Util::UFixedPoint<10, 2>::fromBits(lrS),
                                            Util::UFixedPoint<10, 2>::fromBits(lrT)};
                break;
            }
            // ignore for now
            case Command::SET_SCISSOR: {
                const auto cmd = makeCommand<Commands::SetScissor, 1>(cmds);
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setScissor"},
                        std::tuple{"rectangle", "{}", cmd.getRectangle()});
                    m_logger->log<Level::MED, Sev::WARNING, Sys::RDP>("Ignoring command {}", cmdType);
                }
                break;
            }
            case Command::SET_COLOR_IMAGE: [[fallthrough]];
            case Command::SET_DEPTH_IMAGE: [[fallthrough]];
            case Command::LOAD_TLUT:
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
