module;
#include <util/defines.hpp>
export module RDP:Commands;

import std;
import Interfaces;
import ISA;
import RdpControl;
import Util;

export namespace RDP {

enum class Command : uint8_t {
    FILL_TRIANGLE = 0x08,
    FILL_TRIANGLE_Z,
    FILL_TRIANGLE_T,
    FILL_TRIANGLE_TZ,
    FILL_TRIANGLE_S,
    FILL_TRIANGLE_SZ,
    FILL_TRIANGLE_ST,
    FILL_TRIANGLE_STZ,

    TEXTURE_RECTANGLE = 0x24,
    TEXTURE_RECTANGLEFLIP,
    SYNC_LOAD,
    SYNC_PIPE,
    SYNC_TILE,
    SYNC_FULL,
    SET_KEY_GB,
    SET_KEY_R,
    SET_CONVERT,
    SET_SCISSOR,
    SET_PRIMITIVE_DEPTH,
    SET_OTHER_MODES,
    LOAD_TLUT,

    SET_TILE_SIZE = 0x32,
    LOAD_BLOCK,
    LOAD_TILE,
    SET_TILE,
    FILL_RECTANGLE,
    SET_FILL_COLOR,
    SET_FOG_COLOR,
    SET_BLEND_COLOR,
    SET_PRIMITIVE_COLOR,
    SET_ENVIRONMENT_COLOR,
    SET_COMBINE_MODE,
    SET_TEXTURE_IMAGE,
    SET_DEPTH_IMAGE,
    SET_COLOR_IMAGE,
};

constexpr auto getCommand(uint64_t data) -> Command {
    return static_cast<Command>((data >> 56) & 0x3F);
}

namespace Commands {

struct FillTriangle {
    uint64_t yh          : 14;
    uint64_t             : 2;
    uint64_t ym          : 14;
    uint64_t             : 2;
    uint64_t yl          : 14;
    uint64_t             : 2;
    uint64_t tile        : 3;
    uint64_t level       : 3;
    uint64_t             : 1;
    uint64_t lmajor      : 1;
    uint64_t zbuffer     : 1;
    uint64_t texture     : 1;
    uint64_t shade       : 1;
    uint64_t command_5_3 : 3;
    uint64_t             : 2;

    uint64_t dxLdyF : 16;
    uint64_t dxLdyI : 14;
    uint64_t        : 2;
    uint64_t xlF    : 16;
    uint64_t xlI    : 12;
    uint64_t        : 4;

    uint64_t dxHdyF : 16;
    uint64_t dxHdyI : 14;
    uint64_t        : 2;
    uint64_t xhF    : 16;
    uint64_t xhI    : 12;
    uint64_t        : 4;

    uint64_t dxMdyF : 16;
    uint64_t dxMdyI : 14;
    uint64_t        : 2;
    uint64_t xmF    : 16;
    uint64_t xmI    : 12;
    uint64_t        : 4;

    constexpr auto getTriangle() const {
        constexpr auto ConvertS11_16 = [](uint64_t value) {
            return Util::toFloat<true, 12, 16>(value);
        };
        constexpr auto ConvertS14_16 = [](uint64_t value) {
            return Util::toFloat<true, 14, 16>(value);
        };
        constexpr auto ConvertS11_2 = [](uint64_t value) {
            return Util::toFloat<true, 12, 2>(value);
        };
        const auto dxdy = ConvertS14_16((dxHdyI << 16) + dxHdyF);
        const auto y0   = ConvertS11_2(yh);
        const auto y1   = ConvertS11_2(ym);
        const auto y2   = ConvertS11_2(yl);
        const auto x0   = ConvertS11_16((xhI << 16) + xhF);
        const auto x1   = ConvertS11_16((xlI << 16) + xlF);
        const auto x2   = x0 + (y2 - y0) * dxdy;
        const auto v0   = Util::Point(x0, y0);
        const auto v1   = Util::Point(x1, y1);
        const auto v2   = Util::Point(x2, y2);
        return Util::Triangle(v0, v1, v2);
    }

    constexpr auto getRenderTriangle() const -> Util::RenderTriangle { // s11.2 format
        constexpr auto signExtendS11_2 = [](uint64_t value) {
            return static_cast<int32_t>(static_cast<uint32_t>(value) << 18) >> 18;
        };
        const auto y0 = signExtendS11_2(yh);
        const auto y1 = signExtendS11_2(ym);
        const auto y2 = signExtendS11_2(yl);
        const auto x1 = Util::toFixedS15_16<true, 12, 16>((xlI << 16) + xlF);
        const auto x  = [this, y0](int32_t y) {
            const auto xh      = Util::toFixedS15_16<true, 12, 16>((xhI << 16) + xhF);
            const auto dxdy    = Util::toFixedS15_16<true, 14, 16>((dxHdyI << 16) + dxHdyF);
            const auto y0Floor = y0 & ~3;
            return xh + static_cast<int32_t>((static_cast<int64_t>(y - y0Floor) * dxdy) >> 2);
        };
        const auto x0 = x(y0);
        const auto x2 = x(y2);
        return {x0, y0, 0, x1, y1, 0, x2, y2, 0};
    }

    struct Cmd {
        uint8_t zbuffer : 1;
        uint8_t texture : 1;
        uint8_t shade   : 1;
        uint8_t         : 5;
    };

    struct Shade {
        uint64_t aI : 9;
        uint64_t    : 7;
        uint64_t bI : 9;
        uint64_t    : 7;
        uint64_t gI : 9;
        uint64_t    : 7;
        uint64_t rI : 9;
        uint64_t    : 7;

        uint64_t daDxI : 16;
        uint64_t dbDxI : 16;
        uint64_t dgDxI : 16;
        uint64_t drDxI : 16;

        uint64_t aF : 16;
        uint64_t bF : 16;
        uint64_t gF : 16;
        uint64_t rF : 16;

        uint64_t daDxF : 16;
        uint64_t dbDxF : 16;
        uint64_t dgDxF : 16;
        uint64_t drDxF : 16;

        uint64_t daDeI : 16;
        uint64_t dbDeI : 16;
        uint64_t dgDeI : 16;
        uint64_t drDeI : 16;

        uint64_t daDyI : 16;
        uint64_t dbDyI : 16;
        uint64_t dgDyI : 16;
        uint64_t drDyI : 16;

        uint64_t daDeF : 16;
        uint64_t dbDeF : 16;
        uint64_t dgDeF : 16;
        uint64_t drDeF : 16;

        uint64_t daDyF : 16;
        uint64_t dbDyF : 16;
        uint64_t dgDyF : 16;
        uint64_t drDyF : 16;
    };
};

struct Texture {
    uint64_t    : 16;
    uint64_t wI : 16;
    uint64_t tI : 16;
    uint64_t sI : 16;

    uint64_t       : 16;
    uint64_t dwDxI : 16;
    uint64_t dtDxI : 16;
    uint64_t dsDxI : 16;

    uint64_t    : 16;
    uint64_t wF : 16;
    uint64_t tF : 16;
    uint64_t sF : 16;

    uint64_t       : 16;
    uint64_t dwDxF : 16;
    uint64_t dtDxF : 16;
    uint64_t dsDxF : 16;

    uint64_t       : 16;
    uint64_t dwDeI : 16;
    uint64_t dtDeI : 16;
    uint64_t dsDeI : 16;

    uint64_t       : 16;
    uint64_t dwDyI : 16;
    uint64_t dtDyI : 16;
    uint64_t dsDyI : 16;

    uint64_t       : 16;
    uint64_t dwDeF : 16;
    uint64_t dtDeF : 16;
    uint64_t dsDeF : 16;

    uint64_t       : 16;
    uint64_t dwDyF : 16;
    uint64_t dtDyF : 16;
    uint64_t dsDyF : 16;
};

struct Depth {
    uint64_t dzdyF : 16;
    uint64_t dzdyI : 16;
    uint64_t dzdeF : 16;
    uint64_t dzdeI : 16;
    uint64_t dzdxF : 16;
    uint64_t dzdxI : 16;
    uint64_t zF    : 16;
    uint64_t zI    : 16;
};

struct TextureRectangle {
    uint64_t uly     : 12;
    uint64_t ulx     : 12;
    uint64_t tile    : 3;
    uint64_t         : 5;
    uint64_t lry     : 12;
    uint64_t lrx     : 12;
    uint64_t command : 6;
    uint64_t         : 2;

    uint64_t dtDy : 16;
    uint64_t dsDx : 16;
    uint64_t t    : 10;
    uint64_t s    : 10;

    constexpr auto getRectangle() const {
        constexpr auto ConvertU10_2 = [](uint64_t value) {
            return Util::toFloat<false, 10, 2>(value);
        };
        const auto v0 = Util::Point(ConvertU10_2(ulx), ConvertU10_2(uly));
        const auto v1 = Util::Point(ConvertU10_2(lrx), ConvertU10_2(lry));
        return Util::Rectangle(v0, v1);
    }

    constexpr auto getRenderTriangles() const -> std::array<Util::RenderTriangle, 2> {
        const auto v0x = Util::toFixedS15_16<false, 10, 2>(ulx);
        const auto v0y = Util::toFixedS15_16<false, 10, 2>(uly);
        const auto v1x = Util::toFixedS15_16<false, 10, 2>(lrx);
        const auto v1y = Util::toFixedS15_16<false, 10, 2>(lry);
        return {
            Util::RenderTriangle{v0x, v0y, 0, v1x - v0x, v0y, 0, v0x, v1y - v0y, 0},
            Util::RenderTriangle{v1x, v1y, 0, v0x, v1y - v0y, 0, v1x - v0x, v0y, 0},
        };
    }
};

struct SyncLoad {
};

struct SyncPipe {
};

struct SyncTile {
};

struct SyncFull {
};

struct SetKeyGB {
};

struct SetKeyR {
};

struct SetConvert {
};

struct SetScissor {
    uint32_t lower_right_y : 12;
    uint32_t lower_right_x : 12;
    uint32_t odd           : 1;
    uint32_t field         : 1;
    uint32_t               : 6;
    uint32_t upper_left_y  : 12;
    uint32_t upper_left_x  : 12;
    uint32_t command       : 6;
    uint32_t               : 2;
};

struct SetPrimitiveDepth {
    uint64_t dz      : 16;
    uint64_t z       : 16;
    uint64_t         : 24;
    uint64_t command : 6;
    uint64_t         : 2;
};

struct SetOtherModes {
    uint64_t alphaCompareEn : 1;
    uint64_t ditherAlphaEn  : 1;
    uint64_t zSourceSel     : 1;
    uint64_t antialiasEn    : 1;
    uint64_t zCompareEn     : 1;
    uint64_t zUpdateEn      : 1;
    uint64_t imageReadEn    : 1;
    uint64_t colorOnCvg     : 1;
    uint64_t cvgDest        : 2;
    uint64_t zMode          : 2;
    uint64_t cvgXAlpha      : 1;
    uint64_t alphaCvgSelect : 1;
    uint64_t forceBlend     : 1;
    uint64_t                : 1;
    uint64_t blM2b1         : 2;
    uint64_t blM2b0         : 2;
    uint64_t blM2a1         : 2;
    uint64_t blM2a0         : 2;
    uint64_t bl1b1          : 2;
    uint64_t blM1b0         : 2;
    uint64_t blM1a1         : 2;
    uint64_t blM1a0         : 2;
    uint64_t                : 4;
    uint64_t alphaDitherSel : 2;
    uint64_t rgbDitherSel   : 2;
    uint64_t keyEn          : 1;
    uint64_t convertOne     : 1;
    uint64_t biLerp1        : 1;
    uint64_t biLerp0        : 1;
    uint64_t midTexel       : 1;
    uint64_t sampleType     : 1;
    uint64_t tlutType       : 1;
    uint64_t tlutEn         : 1;
    uint64_t texLodEn       : 1;
    uint64_t sharpenTexEn   : 1;
    uint64_t detailTexEn    : 1;
    uint64_t perspTexEn     : 1;
    uint64_t cycleType      : 2;
    uint64_t                : 1;
    uint64_t atomicPrim     : 1;
    uint64_t command        : 6;
    uint64_t                : 2;
};

struct LoadTLUT {
};

struct SetTileSize {
    uint64_t lowerRightT : 12;
    uint64_t lowerRightS : 12;
    uint64_t index       : 3;
    uint64_t             : 5;
    uint64_t upperLeftT  : 12;
    uint64_t upperLeftS  : 12;
    uint64_t command     : 6;
    uint64_t             : 2;
};

struct LoadBlock {
    uint64_t dxt         : 12;
    uint64_t lowerRightS : 12;
    uint64_t tile        : 3;
    uint64_t             : 5;
    uint64_t upperLeftT  : 12;
    uint64_t upperLeftS  : 12;
    uint64_t command     : 6;
    uint64_t             : 2;
};

struct LoadTile {
};

struct SetTile {
    uint64_t shiftS  : 4;
    uint64_t maskS   : 4;
    uint64_t mirrorS : 1;
    uint64_t clampS  : 1;
    uint64_t shiftT  : 4;
    uint64_t maskT   : 4;
    uint64_t mirrorT : 1;
    uint64_t clampT  : 1;
    uint64_t palette : 4;
    uint64_t index   : 3;
    uint64_t         : 5;
    uint64_t address : 9;
    uint64_t line    : 9;
    uint64_t         : 1;
    uint64_t size    : 2;
    uint64_t format  : 3;
    uint64_t command : 6;
    uint64_t         : 2;
};

struct FillRectangle {
    uint64_t upperLeftY  : 12;
    uint64_t upperLeftX  : 12;
    uint64_t             : 8;
    uint64_t lowerRightY : 12;
    uint64_t lowerRightX : 12;
    uint64_t command     : 6;
    uint64_t             : 2;

    constexpr auto getRectangle() const {
        constexpr auto ConvertU10_2 = [](uint64_t value) {
            return Util::toFloat<false, 10, 2>(value);
        };
        const auto v0 = Util::Point(ConvertU10_2(upperLeftX), ConvertU10_2(upperLeftY));
        const auto v1 = Util::Point(ConvertU10_2(lowerRightX), ConvertU10_2(lowerRightY));
        return Util::Rectangle(v0, v1);
    }

    constexpr auto getRenderTriangles() const -> std::array<Util::RenderTriangle, 2> {
        const auto v0x = Util::toFixedS15_16<false, 10, 2>(upperLeftX);
        const auto v0y = Util::toFixedS15_16<false, 10, 2>(upperLeftY);
        const auto v1x = Util::toFixedS15_16<false, 10, 2>(lowerRightX);
        const auto v1y = Util::toFixedS15_16<false, 10, 2>(lowerRightY);
        return {
            Util::RenderTriangle{v0x, v0y, 0, v1x - v0x, v0y, 0, v0x, v1y - v0y, 0},
            Util::RenderTriangle{v1x, v1y, 0, v0x, v1y - v0y, 0, v1x - v0x, v0y, 0},
        };
    }
};

struct SetFillColor {
    uint64_t color   : 32;
    uint64_t         : 24;
    uint64_t command : 6;
    uint64_t         : 2;
};

struct SetFogColor {
};

struct SetBlendColor {
    uint64_t alpha   : 8;
    uint64_t blue    : 8;
    uint64_t green   : 8;
    uint64_t red     : 8;
    uint64_t         : 24;
    uint64_t command : 6;
    uint64_t         : 2;
};

struct SetPrimitiveColor {
    uint64_t alpha       : 8;
    uint64_t blue        : 8;
    uint64_t green       : 8;
    uint64_t red         : 8;
    uint64_t primLodFrac : 8;
    uint64_t minLevel    : 8;
    uint64_t             : 8;
    uint64_t command     : 6;
    uint64_t             : 2;
};

struct SetEnvironmentColor {
    uint64_t alpha   : 8;
    uint64_t blue    : 8;
    uint64_t green   : 8;
    uint64_t red     : 8;
    uint64_t         : 24;
    uint64_t command : 6;
    uint64_t         : 2;
};

struct SetCombineMode {
    uint64_t alphaD1 : 3;
    uint64_t alphaB1 : 3;
    uint64_t rgbD1   : 3;
    uint64_t alphaD0 : 3;
    uint64_t alphaB0 : 3;
    uint64_t rgbD0   : 3;
    uint64_t alphaC1 : 3;
    uint64_t alphaA1 : 3;
    uint64_t rgbB1   : 4;
    uint64_t rgbB0   : 4;
    uint64_t rgbC1   : 5;
    uint64_t rgbA1   : 4;
    uint64_t alphaC0 : 3;
    uint64_t alphaA0 : 3;
    uint64_t rgbC0   : 5;
    uint64_t rgbA0   : 4;
    uint64_t command : 6;
    uint64_t         : 2;

    enum class Base : uint8_t {
        COMBINED,
        TEX0,
        TEX1,
        PRIMITIVE,
        SHADE,
        ENVIRONMENT,
        ONE,
        ZERO,
    };

    enum class RgbA : uint8_t {
        COMBINED,
        TEX0,
        TEX1,
        PRIMITIVE,
        SHADE,
        ENVIRONMENT,
        ONE,
        NOISE,
        ZERO,
    };
    using AlphaA = Base;
    enum class RgbB : uint8_t {
        COMBINED,
        TEX0,
        TEX1,
        PRIMITIVE,
        SHADE,
        ENVIRONMENT,
        CENTER,
        K4,
        ZERO,
    };
    using AlphaB = Base;
    enum class RgbC : uint8_t {
        COMBINED,
        TEX0,
        TEX1,
        PRIMITIVE,
        SHADE,
        ENVIRONMENT,
        SCALE,
        COMBINED_ALPHA,
        TEX0_ALPHA,
        TEX1_ALPHA,
        PRIMITIVE_ALPHA,
        SHADE_ALPHA,
        ENVIRONMENT_ALPHA,
        LOD_FRACTION,
        PRIM_LOD_FRAC,
        K5,
        ZERO
    };
    enum class AlphaC : uint8_t {
        LOD_FRACTION,
        TEX0,
        TEX1,
        PRIMITIVE,
        SHADE,
        ENVIRONMENT,
        PRIM_LOD_FRAC,
        ZERO
    };
    using RgbD   = Base;
    using AlphaD = Base;
};

struct SetTextureImage {
    uint64_t dramAddress : 24;
    uint64_t             : 8;
    uint64_t width       : 10;
    uint64_t             : 9;
    uint64_t size        : 2;
    uint64_t format      : 3;
    uint64_t command     : 6;
    uint64_t             : 2;
};

struct SetDepthImage {
};

struct SetColorImage {
    uint64_t dramAddress : 24;
    uint64_t             : 8;
    uint64_t width       : 10;
    uint64_t             : 9;
    uint64_t size        : 2;
    uint64_t format      : 3;
    uint64_t command     : 6;
    uint64_t             : 2;
};

} // namespace Commands
} // namespace RDP

STD_FORMATTER_ENUM(RDP::Command, [](auto&& e) { return Util::enumName(e).value_or("NOP"); });