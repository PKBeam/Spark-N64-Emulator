module;
#include <util/defines.hpp>
#include <rdp_gfx_backend/gfxBackend.hpp>
export module RDP:RDP;

import std;
import Interfaces;
import InterfaceTypes;
import ISA;
import MemoryTypes;
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
        Memory::Memory*               memory,
        GfxBackend*                   gfxBackend)
        : m_logger(logger),
          m_rdpControl(rdpControl),
          m_mipsInterface(mipsInterface),
          m_memory(memory),
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
    auto flushBackend() -> void;

    std::shared_ptr<Util::Logger> m_logger;
    ::RDP::Control*               m_rdpControl{};
    Interfaces::MipsInterface*    m_mipsInterface{};
    Memory::Memory*               m_memory{};
    GfxBackend*                   m_gfxBackend{};
    std::byte*                    m_textureMemory{};

    TextureImage        m_textureImage{};
    std::array<Tile, 8> m_tiles{};

    std::size_t           m_syncs{};
    int                   m_terminateAfterSyncs{-1};
    std::function<void()> m_syncCallback{};
    std::size_t           m_syncCallbackCount{};

    std::bitset<8> m_tileUsedThisDraw{};

    Commands::SetCombineMode m_combineMode{};
    CombineInputs::Uniform   m_combineInputs{};
    Commands::SetOtherModes  m_mode{};
    uint32_t                 m_blend{};
    uint32_t                 m_fog{};
    uint32_t                 m_fill{};
};

auto RDP::flushBackend() -> void {
    auto inputs = m_combineMode.getSelects();
    if (m_mode.cycleType != Commands::SetOtherModes::CYCLE_2) {
        // no-op the first cycle
        inputs.rgb[0].c   = CombineInputs::Source::ZERO;
        inputs.alpha[0].c = CombineInputs::Source::ZERO;
        inputs.rgb[0].d   = CombineInputs::Source::ZERO;
        inputs.alpha[0].d = CombineInputs::Source::ZERO;
    }
    inputs.uniform = m_combineInputs;
    m_gfxBackend->setCombineInputs(inputs);
    for (auto tileIndex = 0uz; tileIndex < m_tileUsedThisDraw.size(); ++tileIndex) {
        if (!m_tileUsedThisDraw[tileIndex]) {
            continue;
        }

        const auto& tile = m_tiles[tileIndex];
        m_gfxBackend->updateTile(
            tileIndex,
            tile.params(),
            m_textureMemory + tile.tmemAddress,
            m_mode.tlutType ? TextureFormat::IA16 : TextureFormat::RGBA16,
            m_textureMemory + 0x800);
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
    m_gfxBackend->startRenderPass(RenderOptions{
        .depthTestEnable  = m_mode.zCompareEn,
        .depthWriteEnable = m_mode.zUpdateEn,
    });
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
            // No-ops
            case Command::SYNC_LOAD: [[fallthrough]];
            case Command::SYNC_TILE: [[fallthrough]];
            case Command::SYNC_PIPE: {
                cmds.pop_front();
                break;
            }
            case Command::SYNC_FULL: {
                m_gfxBackend->completeRenderFrame();
                m_mipsInterface->setInterrupt<^^Interfaces::MI_INTERRUPT::dp>(true);
                m_syncs++;
                cmds.pop_front();
                if (m_syncCallback && m_syncs == m_syncCallbackCount) {
                    m_syncCallback();
                }
                if (static_cast<int>(m_syncs) == m_terminateAfterSyncs) {
                    throw Util::Error("Reached termination condition after {} syncs", m_syncs);
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

                const auto cmd   = makeCommand<Commands::FillTriangle, 4>(cmds);
                auto       tri   = cmd.getTriangle();
                auto       shade = Util::Point<std::array<Util::SFixedPoint<16, 16>, 4>>{};
                if (cmdHeader.shade) {
                    const auto shadeCmd = makeCommand<Commands::FillTriangle::Shade, 8>(cmds);
                    shade               = shadeCmd.getShade(tri);
                }

                auto texCoords = Util::RenderTriangle{};
                if (cmdHeader.texture) {
                    const auto textureCmd = makeCommand<Commands::FillTriangle::Texture, 8>(cmds);
                    texCoords             = textureCmd.getTexCoords(tri);
                    if (!m_mode.perspTexEn) {
                        texCoords.setZValues(TexelW_NoPerspectiveDivide);
                    }
                }
                if (cmdHeader.zbuffer) {
                    const auto depthCmd = makeCommand<Commands::FillTriangle::Depth, 2>(cmds);
                    depthCmd.setDepth(tri);
                }

                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "drawTriangle"},
                        std::tuple{"tile", "{}", cmd.tile},
                        std::tuple{"coords", "{}", tri});
                    if (cmdHeader.texture) {
                        m_logger->log<Level::MED, Sys::RDP>(
                            std::tuple{"op", "triangleTexture"},
                            std::tuple{"coords", "{}", texCoords});
                    }
                }
                m_tileUsedThisDraw.set(cmd.tile);
                m_gfxBackend->addTriangle(cmd.tile, tri.bytes(), shade.bytes(), texCoords.bytes());
                flushBackend();
                break;
            }
            case Command::FILL_RECTANGLE: {
                const auto cmd  = makeCommand<Commands::FillRectangle, 1>(cmds);
                const auto tris = cmd.getTriangles(); // todo fill optimisation in vk
                // m_gfxBackend->addTriangle(tris[0].bytes(), nullptr, nullptr, 0);
                // m_gfxBackend->addTriangle(tris[1].bytes(), nullptr, nullptr, 0);
                // flushBackend();
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "drawRectangle"},
                        std::tuple{"coords", "{}", cmd.getRectangle()});
                }
                break;
            }
            case Command::TEXTURE_RECTANGLE: {
                const auto cmd     = makeCommand<Commands::TextureRectangle, 2>(cmds);
                auto [coords, uvs] = cmd.getTriangles();
                if (!m_mode.perspTexEn) { // no correction - set w to 1
                    for (auto& uv : uvs) {
                        uv.setZValues(TexelW_NoPerspectiveDivide);
                    }
                }
                m_tileUsedThisDraw.set(cmd.tile);
                m_gfxBackend->addTriangle(cmd.tile, coords[0].bytes(), nullptr, uvs[0].bytes());
                m_gfxBackend->addTriangle(cmd.tile, coords[1].bytes(), nullptr, uvs[1].bytes());
                flushBackend();
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

                m_tiles[cmd.index] = Tile{
                    .format      = {cmd.format, cmd.size},
                    .extent      = {Util::Fxp_0, Util::Fxp_0, Util::Fxp_0, Util::Fxp_0},
                    .lineSize    = cmd.line * sizeof(uint64_t),
                    .tmemAddress = cmd.address * sizeof(uint64_t),
                    .subPalette  = cmd.palette,
                    .s =
                        {
                            .shift  = static_cast<int8_t>(cmd.shiftS > 10 ? 16 - cmd.shiftS : -static_cast<int>(cmd.shiftS)),
                            .mirror = static_cast<uint8_t>(cmd.mirrorS),
                            .clamp  = static_cast<uint8_t>(cmd.clampS),
                            .mask   = ((cmd.maskS == 0 ? 0xFFFF : ((1u << cmd.maskS) - 1)) << 16) | 0xFFFF,
                            .offset = 0,
                        },
                    .t =
                        {
                            .shift  = static_cast<int8_t>(cmd.shiftT > 10 ? 16 - cmd.shiftT : -static_cast<int>(cmd.shiftT)),
                            .mirror = static_cast<uint8_t>(cmd.mirrorT),
                            .clamp  = static_cast<uint8_t>(cmd.clampT),
                            .mask   = ((cmd.maskT == 0 ? 0xFFFF : ((1u << cmd.maskT) - 1)) << 16) | 0xFFFF,
                            .offset = 0,
                        },
                };
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setTile"},
                        std::tuple{"tile", "{}", cmd.index},
                        std::tuple{"address", HEXFMT12, cmd.address * sizeof(uint64_t)},
                        std::tuple{"lineSize", HEXFMT12, cmd.line * sizeof(uint64_t)},
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
                m_tiles[cmd.index].extent   = extent;
                m_tiles[cmd.index].s.offset = Util::UFixedPoint<16, 16>(extent.ulS).bits();
                m_tiles[cmd.index].t.offset = Util::UFixedPoint<16, 16>(extent.ulT).bits();
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
                    .imageWidth = static_cast<uint16_t>(cmd.width + 1),
                    .rdramAddr  = cmd.dramAddress,
                };
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "setTextureImage"},
                        std::tuple{"addr", HEXFMT32, cmd.dramAddress},
                        std::tuple{"pixelSize", "{}", cmd.size},
                        std::tuple{"imageWidth", "{}", cmd.width + 1});
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
                    const auto value    = Util::byteswapIfLittleEndian(m_memory->read<uint64_t>(rdramBaseAddr + offset));
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

                const auto tmemBaseAddr = m_tiles[cmd.tile].tmemAddress;

                const auto texelSizeBits = m_textureImage.pixelSize.bitsPerPixel();
                const auto texelOffset   = ulT * m_textureImage.imageWidth + ulS;
                const auto rdramBaseAddr = m_textureImage.rdramAddr + ((texelOffset * texelSizeBits) / 8);

                const auto tmemLineSize = m_tiles[cmd.tile].correctedLineSize();

                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "loadTile"},
                        std::tuple{"tile", "{}", cmd.tile},
                        std::tuple{"rdramAddr", HEXFMT32, rdramBaseAddr},
                        std::tuple{"coords", "({}, {}), ({}, {})", ulS & 0xFFF, ulT & 0xFFF, lrS & 0xFFF, lrT & 0xFFF});
                }

                const auto rdramRowWidth = m_textureImage.imageWidth * texelSizeBits / 8;
                const auto copySize      = (width * texelSizeBits + 7) / 8;
                for (const auto row : std::views::iota(0, height)) {
                    const auto tmemAddr  = tmemBaseAddr + (row * tmemLineSize);
                    const auto rdramAddr = rdramBaseAddr + (row * rdramRowWidth);
                    for (const auto byte : std::views::iota(0uz, copySize)) {
                        const auto value = std::bit_cast<std::byte>(m_memory->read<uint8_t>(rdramAddr + byte));
                        IF_LOG_ENABLED(m_logger) {
                            m_logger->log<Level::LOW, Sys::RDP>(
                                std::tuple{"op", "w"},
                                std::tuple{"addr", HEXFMT12, tmemAddr + byte},
                                std::tuple{"value", HEXFMT8, std::bit_cast<uint8_t>(value)});
                        }
                        m_textureMemory[(tmemAddr + byte) & 0xFFF] = value;
                    }
                }
                m_tiles[cmd.tile].extent = {Util::UFixedPoint<10, 2>::fromBits(ulS),
                                            Util::UFixedPoint<10, 2>::fromBits(ulT),
                                            Util::UFixedPoint<10, 2>::fromBits(lrS),
                                            Util::UFixedPoint<10, 2>::fromBits(lrT)};
                break;
            }
            case Command::LOAD_TLUT: {
                const auto     cmd       = makeCommand<Commands::LoadTLUT, 1>(cmds);
                const auto     width     = ((cmd.lowerRightS >> 2) - (cmd.upperLeftS >> 2)) + 1;
                constexpr auto texelSize = 2uz; // TLUTs are always 16-bit

                const auto tmemBaseAddr  = m_tiles[cmd.tile].tmemAddress;
                const auto rdramBaseAddr = m_textureImage.rdramAddr;

                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sys::RDP>(
                        std::tuple{"op", "loadTLUT"},
                        std::tuple{"tile", "{}", cmd.tile},
                        std::tuple{"rdramAddr", HEXFMT32, rdramBaseAddr},
                        std::tuple{"size", "{}", width});
                }

                for (const auto texelIndex : std::views::iota(0, width)) {
                    const auto rdramOffset = texelIndex * texelSize;
                    const auto tmemOffset  = 4 * rdramOffset;
                    const auto tmemAddr    = (tmemBaseAddr + tmemOffset) & 0xFFF;
                    const auto rdramAddr   = rdramBaseAddr + rdramOffset;

                    const auto texel = Util::byteswapIfLittleEndian(m_memory->read<uint16_t>(rdramAddr));
                    for (const auto i : std::views::iota(0, 4)) {
                        std::memcpy(m_textureMemory + tmemAddr + i * texelSize, &texel, texelSize);
                    }
                    IF_LOG_ENABLED(m_logger) {
                        m_logger->log<Level::LOW, Sys::RDP>(
                            std::tuple{"op", "w"},
                            std::tuple{"addr", HEXFMT12, tmemAddr},
                            std::tuple{"value", HEXFMT16, Util::byteswapIfLittleEndian(texel)});
                    }
                }
                m_tiles[cmd.tile].extent = {Util::UFixedPoint<10, 2>::fromBits(cmd.upperLeftS),
                                            Util::UFixedPoint<10, 2>::fromBits(cmd.upperLeftT),
                                            Util::UFixedPoint<10, 2>::fromBits(cmd.lowerRightS),
                                            Util::UFixedPoint<10, 2>::fromBits(cmd.lowerRightT)};
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
            case Command::SET_DEPTH_IMAGE:
                cmds.pop_front();
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::MED, Sev::WARNING, Sys::RDP>("Ignoring command {}", cmdType);
                }
                break;
            default:
                throw Util::Error("RDP unimplemented command {}", cmdType);
        }
    }
}
} // namespace RDP
