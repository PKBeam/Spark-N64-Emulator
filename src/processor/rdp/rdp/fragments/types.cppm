module;
#include <util/defines.hpp>
#include <processor/rdp/gfx/gfxBackend.hpp>
export module RDP:Types;

import std;
import Interfaces;
import ISA;
import RdpControl;
import Util;

export namespace RDP {

// see rdp frag shader for details
constexpr auto TexelW_NoPerspectiveDivide = Util::SFixedPoint<16, 16>::fromValue(1024);

struct PixelFormat {
    enum Value : uint8_t {
        RGBA,
        YUV,
        CI,
        IA,
        I
    };
    Value value;
    constexpr PixelFormat() : value(Value::RGBA) {}
    constexpr PixelFormat(uint8_t v) : value(static_cast<Value>(std::min(v, uint8_t(4)))) {}
    constexpr operator uint8_t() const {
        return static_cast<uint8_t>(value);
    }
};

struct PixelSize {
    uint8_t        value;
    constexpr auto bitsPerPixel() const -> std::size_t {
        return 4 << value;
    }
    constexpr auto bytesPerPixel() const -> std::size_t {
        return bitsPerPixel() / 8;
    }
};

struct TileFormat {
    PixelFormat format;
    PixelSize   size;
};

struct TextureImage {
    PixelSize pixelSize;
    uint16_t  imageWidth;
    uint32_t  rdramAddr;
};

struct Tile {
    struct Extent {
        Util::UFixedPoint<10, 2> ulS;
        Util::UFixedPoint<10, 2> ulT;
        Util::UFixedPoint<10, 2> lrS;
        Util::UFixedPoint<10, 2> lrT;

        constexpr auto width() const -> std::size_t {
            return static_cast<std::size_t>(1 + (lrS - ulS).integer());
        }

        constexpr auto height() const -> std::size_t {
            return static_cast<std::size_t>(1 + (lrT - ulT).integer());
        }
    };
    TileFormat           format;
    Extent               extent;
    std::size_t          lineSize;
    std::size_t          tmemAddress;
    uint8_t              subPalette;
    ::RDP::SamplerParams s;
    ::RDP::SamplerParams t;

    // RDP stores RGBA32 as split RG16 and BA16. This is a hack to correct the line size.
    // TODO: need long term solution - games may expect TMEM to be laid out as rgrgbaba?
    constexpr auto correctedLineSize() const -> std::size_t {
        return (format.size.bitsPerPixel() == 32 ? 2 : 1) * lineSize;
    }

    constexpr auto params() const -> ::RDP::TileParams {
        return {
            .width      = static_cast<uint32_t>(extent.width()),
            .height     = static_cast<uint32_t>(extent.height()),
            .stride     = static_cast<uint32_t>(correctedLineSize()),
            .format     = textureFormat(),
            .s          = s,
            .t          = t,
            .subPalette = subPalette,
        };
    }

    constexpr auto isPaletted() const -> bool {
        return format.format == PixelFormat::CI;
    }

    constexpr auto textureFormat() const -> ::RDP::TextureFormat {
        const auto format = this->format.format;
        const auto size   = this->format.size;
        using namespace RDP;
        switch (format) {
            case PixelFormat::RGBA:
                switch (size.bitsPerPixel()) {
                    case 16: return TextureFormat::RGBA16;
                    case 32: return TextureFormat::RGBA32;
                    default: return TextureFormat::INVALID;
                }
            case PixelFormat::YUV:
                switch (size.bitsPerPixel()) {
                    case 16: return TextureFormat::YUV16;
                    default: return TextureFormat::INVALID;
                }
            case PixelFormat::CI:
                switch (size.bitsPerPixel()) {
                    case 4: return TextureFormat::CI4;
                    case 8: return TextureFormat::CI8;
                    default: return TextureFormat::INVALID;
                }
            case PixelFormat::IA:
                switch (size.bitsPerPixel()) {
                    case 4: return TextureFormat::IA4;
                    case 8: return TextureFormat::IA8;
                    case 16: return TextureFormat::IA16;
                    default: return TextureFormat::INVALID;
                }
            default: // PixelFormat::I
                switch (size.bitsPerPixel()) {
                    case 4: return TextureFormat::I4;
                    case 8: return TextureFormat::I8;
                    default: return TextureFormat::INVALID;
                }
        }
    }
};

namespace CombineModeInputs {
// clang-format off
    enum class Base : uint8_t {
        COMBINED    [[=^^CombineInputs::Source::COMBINED]],
        TEX0        [[=^^CombineInputs::Source::TEX0]],
        TEX1        [[=^^CombineInputs::Source::TEX1]],
        PRIMITIVE   [[=^^CombineInputs::Source::PRIMITIVE]],
        SHADE       [[=^^CombineInputs::Source::SHADE]],
        ENVIRONMENT [[=^^CombineInputs::Source::ENVIRONMENT]],
        ONE         [[=^^CombineInputs::Source::ONE]],
        ZERO        [[=^^CombineInputs::Source::ZERO]],
    };

    enum class RgbA : uint8_t {
        COMBINED    [[=^^CombineInputs::Source::COMBINED]],
        TEX0        [[=^^CombineInputs::Source::TEX0]],
        TEX1        [[=^^CombineInputs::Source::TEX1]],
        PRIMITIVE   [[=^^CombineInputs::Source::PRIMITIVE]],
        SHADE       [[=^^CombineInputs::Source::SHADE]],
        ENVIRONMENT [[=^^CombineInputs::Source::ENVIRONMENT]],
        ONE         [[=^^CombineInputs::Source::ONE]],
        NOISE       [[=^^CombineInputs::Source::NOISE]],
        ZERO        [[=^^CombineInputs::Source::ZERO]],
    };
    using AlphaA = Base;
    enum class RgbB : uint8_t {
        COMBINED    [[=^^CombineInputs::Source::COMBINED]],
        TEX0        [[=^^CombineInputs::Source::TEX0]],
        TEX1        [[=^^CombineInputs::Source::TEX1]],
        PRIMITIVE   [[=^^CombineInputs::Source::PRIMITIVE]],
        SHADE       [[=^^CombineInputs::Source::SHADE]],
        ENVIRONMENT [[=^^CombineInputs::Source::ENVIRONMENT]],
        CENTER      [[=^^CombineInputs::Source::CENTER]],
        K4          [[=^^CombineInputs::Source::K4]],
        ZERO        [[=^^CombineInputs::Source::ZERO]],
    };
    using AlphaB = Base;
    enum class RgbC : uint8_t {
        COMBINED            [[=^^CombineInputs::Source::COMBINED]],
        TEX0                [[=^^CombineInputs::Source::TEX0]],
        TEX1                [[=^^CombineInputs::Source::TEX1]],
        PRIMITIVE           [[=^^CombineInputs::Source::PRIMITIVE]],
        SHADE               [[=^^CombineInputs::Source::SHADE]],
        ENVIRONMENT         [[=^^CombineInputs::Source::ENVIRONMENT]],
        SCALE               [[=^^CombineInputs::Source::SCALE]],
        COMBINED_ALPHA      [[=^^CombineInputs::Source::COMBINED_ALPHA]],
        TEX0_ALPHA          [[=^^CombineInputs::Source::TEX0_ALPHA]],
        TEX1_ALPHA          [[=^^CombineInputs::Source::TEX1_ALPHA]],
        PRIMITIVE_ALPHA     [[=^^CombineInputs::Source::PRIMITIVE_ALPHA]],
        SHADE_ALPHA         [[=^^CombineInputs::Source::SHADE_ALPHA]],
        ENVIRONMENT_ALPHA   [[=^^CombineInputs::Source::ENVIRONMENT_ALPHA]],
        LOD_FRACTION        [[=^^CombineInputs::Source::LOD_FRACTION]],
        PRIM_LOD_FRAC       [[=^^CombineInputs::Source::PRIM_LOD_FRAC]],
        K5                  [[=^^CombineInputs::Source::K5]],
        ZERO                [[=^^CombineInputs::Source::ZERO]],
    };
    enum class AlphaC : uint8_t {
        LOD_FRACTION    [[=^^CombineInputs::Source::LOD_FRACTION]],
        TEX0            [[=^^CombineInputs::Source::TEX0]],
        TEX1            [[=^^CombineInputs::Source::TEX1]],
        PRIMITIVE       [[=^^CombineInputs::Source::PRIMITIVE]],
        SHADE           [[=^^CombineInputs::Source::SHADE]],
        ENVIRONMENT     [[=^^CombineInputs::Source::ENVIRONMENT]],
        PRIM_LOD_FRAC   [[=^^CombineInputs::Source::PRIM_LOD_FRAC]],
        ZERO            [[=^^CombineInputs::Source::ZERO]]
    };
    using RgbD   = Base;
    using AlphaD = Base;
// clang-format on
} // namespace CombineModeInputs

} // namespace RDP

STD_FORMATTER_ENUM_NAME(RDP::PixelFormat::Value);
template <>
struct std::formatter<RDP::PixelFormat> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const RDP::PixelFormat& value, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", value.value);
    }
};
template <>
struct std::formatter<RDP::TileFormat> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const RDP::TileFormat& value, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}{}", value.format, value.size.bitsPerPixel());
    }
};
template <>
struct std::formatter<RDP::Tile::Extent> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    constexpr auto format(const RDP::Tile::Extent& value, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "(" UPIXFMT ", " UPIXFMT "), (" UPIXFMT ", " UPIXFMT ")", static_cast<float>(value.ulS), static_cast<float>(value.ulT), static_cast<float>(value.lrS), static_cast<float>(value.lrT));
    }
};