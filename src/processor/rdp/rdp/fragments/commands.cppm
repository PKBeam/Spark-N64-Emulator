module;
#include <util/defines.hpp>
#include <processor/rdp/gfx/gfxBackend.hpp>
export module RDP:Commands;

import std;
import Util;
import :Types;

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

    using TriangleProcessResult = std::tuple<
        Util::RenderTriangle,                  // primary triangle
        std::optional<Util::RenderTriangle>,   // additional triangle to adjust for scanline raster accuracy
        Util::Point<Util::SFixedPoint<16, 16>> // reference point for shade/texture
        >;

    constexpr auto getTriangle() const -> TriangleProcessResult {
        const auto y0 = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<12, 2>::fromBits(yh));
        const auto y1 = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<12, 2>::fromBits(ym));
        const auto y2 = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<12, 2>::fromBits(yl));

        const auto xh = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<12, 16>(xhI, xhF));
        const auto xl = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<12, 16>(xlI, xlF));
        const auto xm = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<12, 16>(xmI, xmF));

        const auto x_dh = [&, this](Util::SFixedPoint<12, 2> y) {
            const auto dxdy = Util::SFixedPoint<14, 16>(dxHdyI, dxHdyF);
            const auto dy   = y - y0.floor();
            const auto dx   = dxdy * dy;
            return static_cast<Util::SFixedPoint<16, 16>>(xh + dx);
        };
        const auto x_dm = [&, this](Util::SFixedPoint<12, 2> y) {
            const auto dxdy = Util::SFixedPoint<14, 16>(dxMdyI, dxMdyF);
            const auto dy   = y - y0.floor();
            const auto dx   = dxdy * dy;
            return static_cast<Util::SFixedPoint<16, 16>>(xm + dx);
        };
        const auto x_dl = [&, this](Util::SFixedPoint<12, 2> y) {
            const auto dxdy = Util::SFixedPoint<14, 16>(dxLdyI, dxLdyF);
            const auto dy   = y - y1;
            const auto dx   = dxdy * dy;
            return static_cast<Util::SFixedPoint<16, 16>>(xl + dx);
        };

        const auto x0_h = static_cast<Util::SFixedPoint<16, 16>>(xh);
        const auto x0_m = static_cast<Util::SFixedPoint<16, 16>>(xm);
        const auto x1   = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<12, 16>(xlI, xlF));
        const auto x2_h = x_dh(y2);
        const auto x2_l = x_dl(y2);

        const auto v0h = Util::Point(x_dh(y0), y0);
        const auto v0m = Util::Point(x_dm(y0), y0);
        const auto v1  = Util::Point(x1, y1);
        const auto v2h = Util::Point(x2_h, y2);
        const auto v2l = Util::Point(x2_l, y2);

        const auto referencePoint = Util::Point(xh, y0.floor());

        if (yh == ym) {
            return {
                Util::RenderTriangle(v0h, v1, v2h),
                Util::RenderTriangle(v1, v2l, v2h),
                referencePoint};
        } else if (yl == ym) {
            const auto zero = Util::Point(Util::Fxp_0, Util::Fxp_0);
            return {
                Util::RenderTriangle(v0h, v1, v2h),
                std::nullopt,
                referencePoint};
        } else {
            return {
                Util::RenderTriangle(v0h, v1, v2h),
                Util::RenderTriangle(v1, v2l, v2h),
                referencePoint};
        }
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

        constexpr auto getShade(const Util::Point<Util::SFixedPoint<16, 16>>& referencePoint, const Util::RenderTriangle& tri) const -> Util::Point<std::array<Util::SFixedPoint<16, 16>, 4>> {
            const auto r = Util::SFixedPoint<9, 16>(rI, rF);
            const auto g = Util::SFixedPoint<9, 16>(gI, gF);
            const auto b = Util::SFixedPoint<9, 16>(bI, bF);
            const auto a = Util::SFixedPoint<9, 16>(aI, aF);

            const auto drdx = Util::SFixedPoint<16, 16>(drDxI, drDxF);
            const auto dgdx = Util::SFixedPoint<16, 16>(dgDxI, dgDxF);
            const auto dbdx = Util::SFixedPoint<16, 16>(dbDxI, dbDxF);
            const auto dadx = Util::SFixedPoint<16, 16>(daDxI, daDxF);

            const auto drdy = Util::SFixedPoint<16, 16>(drDyI, drDyF);
            const auto dgdy = Util::SFixedPoint<16, 16>(dgDyI, dgDyF);
            const auto dbdy = Util::SFixedPoint<16, 16>(dbDyI, dbDyF);
            const auto dady = Util::SFixedPoint<16, 16>(daDyI, daDyF);

            const auto rgba = [&, this](Util::SFixedPoint<16, 16> x, Util::SFixedPoint<16, 16> y) -> std::array<Util::SFixedPoint<16, 16>, 4> {
                const auto x0 = tri.v0().x();
                const auto y0 = tri.v0().y();

                const auto dx = x - referencePoint.x();
                const auto dy = y - referencePoint.y();

                const auto dr = drdx * dx + drdy * dy;
                const auto dg = dgdx * dx + dgdy * dy;
                const auto db = dbdx * dx + dbdy * dy;
                const auto da = dadx * dx + dady * dy;

                const auto rOut = Util::SFixedPoint<32, 32>(r) + dr;
                const auto gOut = Util::SFixedPoint<32, 32>(g) + dg;
                const auto bOut = Util::SFixedPoint<32, 32>(b) + db;
                const auto aOut = Util::SFixedPoint<32, 32>(a) + da;

                return {rOut, gOut, bOut, aOut};
            };

            const auto shade0 = rgba(tri.v0().x(), tri.v0().y());
            const auto shade1 = rgba(tri.v1().x(), tri.v1().y());
            const auto shade2 = rgba(tri.v2().x(), tri.v2().y());

            return Util::Point{shade0, shade1, shade2};
        }
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

        constexpr auto getTexCoords(const Util::Point<Util::SFixedPoint<16, 16>>& referencePoint, const Util::RenderTriangle& tri) const -> Util::RenderTriangle {
            const auto s = Util::SFixedPoint<16, 16>(sI, sF);
            const auto t = Util::SFixedPoint<16, 16>(tI, tF);
            const auto w = Util::SFixedPoint<16, 16>(wI, wF);

            const auto dsdx = Util::SFixedPoint<16, 16>(dsDxI, dsDxF);
            const auto dsdy = Util::SFixedPoint<16, 16>(dsDyI, dsDyF);
            const auto dtdx = Util::SFixedPoint<16, 16>(dtDxI, dtDxF);
            const auto dtdy = Util::SFixedPoint<16, 16>(dtDyI, dtDyF);
            const auto dwdx = Util::SFixedPoint<16, 16>(dwDxI, dwDxF);
            const auto dwdy = Util::SFixedPoint<16, 16>(dwDyI, dwDyF);

            const auto st = [&, this](Util::SFixedPoint<16, 16> x, Util::SFixedPoint<16, 16> y) -> Util::Point<Util::SFixedPoint<16, 16>> {
                const auto x0 = tri.v0().x();
                const auto y0 = tri.v0().y();

                const auto dx = x - referencePoint.x();
                const auto dy = y - referencePoint.y();

                const auto dsX = dsdx * dx;
                const auto dsY = dsdy * dy;
                const auto dtX = dtdx * dx;
                const auto dtY = dtdy * dy;
                const auto dwX = dwdx * dx;
                const auto dwY = dwdy * dy;

                const auto sOut = Util::SFixedPoint<32, 32>(s) + dsX + dsY;
                const auto tOut = Util::SFixedPoint<32, 32>(t) + dtX + dtY;
                const auto wOut = Util::SFixedPoint<32, 32>(w) + dwX + dwY;
                return Util::Point{sOut, tOut, wOut};
            };
            auto s0 = st(tri.v0().x(), tri.v0().y());
            auto s1 = st(tri.v1().x(), tri.v1().y());
            auto s2 = st(tri.v2().x(), tri.v2().y());

            return Util::RenderTriangle(s0, s1, s2);
        }
    };

    struct Depth {
        uint64_t dzdxF : 16;
        uint64_t dzdxI : 16;
        uint64_t zF    : 16;
        uint64_t zI    : 16;
        uint64_t dzdyF : 16;
        uint64_t dzdyI : 16;
        uint64_t dzdeF : 16;
        uint64_t dzdeI : 16;

        constexpr auto setDepth(Util::RenderTriangle& tri) const {
            const auto z    = Util::SFixedPoint<16, 16>(zI, zF);
            const auto dzdx = Util::SFixedPoint<16, 16>(dzdxI, dzdxF);
            const auto dzdy = Util::SFixedPoint<16, 16>(dzdyI, dzdyF);

            const auto zFor = [&, this](Util::SFixedPoint<16, 16> x, Util::SFixedPoint<16, 16> y) -> Util::SFixedPoint<16, 16> {
                const auto x0 = tri.v0().x();
                const auto y0 = tri.v0().y();

                const auto dx = x - x0;
                const auto dy = y - y0.floor();

                const auto dzX = dzdx * dx;
                const auto dzY = dzdy * dy;

                const auto zOut = z + dzX + dzY;
                return zOut;
            };
            const auto z0 = zFor(tri.v0().x(), tri.v0().y());
            const auto z1 = zFor(tri.v1().x(), tri.v1().y());
            const auto z2 = zFor(tri.v2().x(), tri.v2().y());
            tri.setZValues(z0, z1, z2);
        }
    };
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
        const auto ulxF = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(ulx));
        const auto lrxF = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(lrx));
        const auto ulyF = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(uly));
        const auto lryF = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(lry));
        const auto v0   = Util::Point(ulxF, ulyF);
        const auto v1   = Util::Point(lrxF, lryF);
        return Util::Rectangle(v0, v1);
    }

    constexpr auto getTriangles() const -> std::pair<std::array<Util::RenderTriangle, 2>, std::array<Util::RenderTriangle, 2>> {
        const auto v0x = static_cast<Util::SFixedPoint<16, 16>>(Util::UFixedPoint<10, 2>::fromBits(ulx));
        const auto v1x = static_cast<Util::SFixedPoint<16, 16>>(Util::UFixedPoint<10, 2>::fromBits(lrx));
        const auto v0y = static_cast<Util::SFixedPoint<16, 16>>(Util::UFixedPoint<10, 2>::fromBits(uly));
        const auto v1y = static_cast<Util::SFixedPoint<16, 16>>(Util::UFixedPoint<10, 2>::fromBits(lry));

        const auto v0 = Util::Point(v0x, v0y);
        const auto v1 = Util::Point(v1x, v0y);
        const auto v2 = Util::Point(v0x, v1y);
        const auto v3 = Util::Point(v1x, v1y);

        const auto coords = std::array<Util::RenderTriangle, 2>{
            Util::RenderTriangle(v0, v1, v3),
            Util::RenderTriangle(v0, v3, v2),
        };

        const auto v0s = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<10, 5>::fromBits(s));
        const auto v0t = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<10, 5>::fromBits(t));

        const auto dsdx = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<5, 10>::fromBits(dsDx));
        const auto dtdy = static_cast<Util::SFixedPoint<16, 16>>(Util::SFixedPoint<5, 10>::fromBits(dtDy));

        const auto st = [this, v0x, v0y, v0s, v0t, dsdx, dtdy](Util::SFixedPoint<16, 16> x, Util::SFixedPoint<16, 16> y) {
            const auto dx = x - v0x;
            const auto dy = y - v0y;

            const auto ds = dsdx * dx;
            const auto dt = dtdy * dy;

            const auto sOut = v0s + ds;
            const auto tOut = v0t + dt;

            return Util::Point{sOut, tOut};
        };

        const auto st0 = Util::Point(v0s, v0t);
        const auto st1 = st(v1.x(), v1.y());
        const auto st2 = st(v2.x(), v2.y());
        const auto st3 = st(v3.x(), v3.y());

        const auto texture = std::array<Util::RenderTriangle, 2>{
            Util::RenderTriangle(st0, st1, st3),
            Util::RenderTriangle(st0, st3, st2),
        };
        return {coords, texture};
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
    uint32_t lowerRightY : 12;
    uint32_t lowerRightX : 12;
    uint32_t odd         : 1;
    uint32_t field       : 1;
    uint32_t             : 6;
    uint32_t upperLeftY  : 12;
    uint32_t upperLeftX  : 12;
    uint32_t command     : 6;
    uint32_t             : 2;

    constexpr auto getRectangle() const {
        const auto v0x = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(upperLeftX));
        const auto v1x = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(upperLeftY));
        const auto v0y = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(lowerRightX));
        const auto v1y = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(lowerRightY));
        const auto v0  = Util::Point(v0x, v0y);
        const auto v1  = Util::Point(v1x, v1y);
        return Util::Rectangle(v0, v1);
    }
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

    enum CycleType {
        CYCLE_1    = 0,
        CYCLE_2    = 1,
        CYCLE_COPY = 2,
        CYCLE_FILL = 3
    };
};

struct LoadTLUT {
    uint64_t lowerRightT : 12;
    uint64_t lowerRightS : 12;
    uint64_t tile        : 3;
    uint64_t             : 5;
    uint64_t upperLeftT  : 12;
    uint64_t upperLeftS  : 12;
    uint64_t command     : 6;
    uint64_t             : 2;
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
    uint64_t lowerRightT : 12;
    uint64_t lowerRightS : 12;
    uint64_t tile        : 3;
    uint64_t             : 5;
    uint64_t upperLeftT  : 12;
    uint64_t upperLeftS  : 12;
    uint64_t command     : 6;
    uint64_t             : 2;
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
        const auto v0x = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(upperLeftX));
        const auto v1x = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(upperLeftY));
        const auto v0y = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(lowerRightX));
        const auto v1y = static_cast<float>(Util::UFixedPoint<10, 2>::fromBits(lowerRightY));
        const auto v0  = Util::Point(v0x, v0y);
        const auto v1  = Util::Point(v1x, v1y);
        return Util::Rectangle(v0, v1);
    }

    constexpr auto getTriangles() const -> std::array<Util::RenderTriangle, 2> {
        const auto v0x = static_cast<Util::SFixedPoint<16, 16>>(Util::UFixedPoint<10, 2>::fromBits(upperLeftX));
        const auto v1x = static_cast<Util::SFixedPoint<16, 16>>(Util::UFixedPoint<10, 2>::fromBits(upperLeftY));
        const auto v0y = static_cast<Util::SFixedPoint<16, 16>>(Util::UFixedPoint<10, 2>::fromBits(lowerRightX));
        const auto v1y = static_cast<Util::SFixedPoint<16, 16>>(Util::UFixedPoint<10, 2>::fromBits(lowerRightY));

        const auto v0 = Util::Point(v0x, v0y);
        const auto v1 = Util::Point(v1x, v0y);
        const auto v2 = Util::Point(v0x, v1y);
        const auto v3 = Util::Point(v1x, v1y);
        return {
            Util::RenderTriangle(v0, v1, v3),
            Util::RenderTriangle(v0, v3, v2),
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
    uint64_t alpha   : 8;
    uint64_t blue    : 8;
    uint64_t green   : 8;
    uint64_t red     : 8;
    uint64_t         : 24;
    uint64_t command : 6;
    uint64_t         : 2;
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
    // clang-format off
    //                  [[   Enum Type                     alpha/rgb              cycle   a/b/c/d component       ]]
    uint64_t alphaD1    [[=^^CombineModeInputs::AlphaD, =^^CombineInputs::alpha, =1uz, =^^CombineInputs::Select::d]] : 3;
    uint64_t alphaB1    [[=^^CombineModeInputs::AlphaB, =^^CombineInputs::alpha, =1uz, =^^CombineInputs::Select::b]] : 3;
    uint64_t rgbD1      [[=^^CombineModeInputs::RgbD,   =^^CombineInputs::rgb,   =1uz, =^^CombineInputs::Select::d]] : 3;
    uint64_t alphaD0    [[=^^CombineModeInputs::AlphaD, =^^CombineInputs::alpha, =0uz, =^^CombineInputs::Select::d]] : 3;
    uint64_t alphaB0    [[=^^CombineModeInputs::AlphaB, =^^CombineInputs::alpha, =0uz, =^^CombineInputs::Select::b]] : 3;
    uint64_t rgbD0      [[=^^CombineModeInputs::RgbD,   =^^CombineInputs::rgb,   =0uz, =^^CombineInputs::Select::d]] : 3;
    uint64_t alphaC1    [[=^^CombineModeInputs::AlphaC, =^^CombineInputs::alpha, =1uz, =^^CombineInputs::Select::c]] : 3;
    uint64_t alphaA1    [[=^^CombineModeInputs::AlphaA, =^^CombineInputs::alpha, =1uz, =^^CombineInputs::Select::a]] : 3;
    uint64_t rgbB1      [[=^^CombineModeInputs::RgbB,   =^^CombineInputs::rgb,   =1uz, =^^CombineInputs::Select::b]] : 4;
    uint64_t rgbB0      [[=^^CombineModeInputs::RgbB,   =^^CombineInputs::rgb,   =0uz, =^^CombineInputs::Select::b]] : 4;
    uint64_t rgbC1      [[=^^CombineModeInputs::RgbC,   =^^CombineInputs::rgb,   =1uz, =^^CombineInputs::Select::c]] : 5;
    uint64_t rgbA1      [[=^^CombineModeInputs::RgbA,   =^^CombineInputs::rgb,   =1uz, =^^CombineInputs::Select::a]] : 4;
    uint64_t alphaC0    [[=^^CombineModeInputs::AlphaC, =^^CombineInputs::alpha, =0uz, =^^CombineInputs::Select::c]] : 3;
    uint64_t alphaA0    [[=^^CombineModeInputs::AlphaA, =^^CombineInputs::alpha, =0uz, =^^CombineInputs::Select::a]] : 3;
    uint64_t rgbC0      [[=^^CombineModeInputs::RgbC,   =^^CombineInputs::rgb,   =0uz, =^^CombineInputs::Select::c]] : 5;
    uint64_t rgbA0      [[=^^CombineModeInputs::RgbA,   =^^CombineInputs::rgb,   =0uz, =^^CombineInputs::Select::a]] : 4;
    uint64_t command                                     : 6;
    uint64_t                                             : 2;
    // clang-format on

    constexpr auto getSelects() -> CombineInputs {
        auto result = CombineInputs();
        template for (constexpr auto member : Util::nonstaticDataMembersOf(^^SetCombineMode)) {
            constexpr auto anns = Util::staticAnnotationsOf(member);
            if constexpr (!anns.empty()) {
                constexpr auto E       = std::meta::extract<std::meta::info>(anns[0]);
                constexpr auto channel = std::meta::extract<std::meta::info>(anns[1]);
                constexpr auto cycle   = std::meta::extract<std::size_t>(anns[2]);
                constexpr auto abcd    = std::meta::extract<std::meta::info>(anns[3]);
                template for (constexpr auto e : Util::staticEnumeratorsOf(E)) {
                    const auto value = this->[:member:];
                    if (value == static_cast<uint64_t>(std::meta::extract<typename[:E:]>(e))) {
                        constexpr auto selectMember        = std::meta::extract<std::meta::info>(Util::annotationOf(e));
                        const auto     source              = static_cast<CombineInputs::Source>([:selectMember:]);
                        result.[:channel:][cycle].[:abcd:] = source;
                    }
                }
            }
        }
        return result;
    }
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
