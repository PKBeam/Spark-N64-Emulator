export module Interfaces:VideoInterfaceTypes;

import std;
import Util;

import :Interface;

export namespace Interfaces {

struct VI_REG_ADDR {
    static constexpr uint32_t BASE = 0x04400000;
    static constexpr uint32_t END  = 0x044FFFFF;
    enum Address : uint32_t {
        VI_CTRL         = 0x04400000,
        VI_ORIGIN       = 0x04400004,
        VI_WIDTH        = 0x04400008,
        VI_V_INTR       = 0x0440000C,
        VI_V_CURRENT    = 0x04400010,
        VI_BURST        = 0x04400014,
        VI_V_TOTAL      = 0x04400018,
        VI_H_TOTAL      = 0x0440001C,
        VI_H_TOTAL_LEAP = 0x04400020,
        VI_H_VIDEO      = 0x04400024,
        VI_V_VIDEO      = 0x04400028,
        VI_V_BURST      = 0x0440002C,
        VI_X_SCALE      = 0x04400030,
        VI_Y_SCALE      = 0x04400034,
        VI_TEST_ADDR    = 0x04400038,
        VI_STAGED_DATA  = 0x0440003C,
    };
};

struct VI_CTRL {
    uint32_t type              : 2;
    uint32_t gammaDitherEnable : 1;
    uint32_t gammaEnable       : 1;
    uint32_t divotEnable       : 1;
    uint32_t vbusClockEnable   : 1;
    uint32_t serrate           : 1;
    uint32_t testMode          : 1;
    uint32_t aaMode            : 2;
    uint32_t                   : 1;
    uint32_t killWe            : 1;
    uint32_t pixelAdvance      : 4;
    uint32_t deditherEnable    : 1;
    uint32_t                   : 15;
};

struct VI_ORIGIN {
    uint32_t origin : 24;
    uint32_t        : 8;
};

struct VI_WIDTH {
    uint32_t width : 12;
    uint32_t       : 20;
};

struct VI_V_INTR {
    uint32_t vIntr : 10;
    uint32_t       : 22;
};

struct VI_V_CURRENT {
    uint32_t field    : 1;
    uint32_t vCurrent : 9;
    uint32_t          : 22;
};

struct VI_BURST {
    uint32_t hsyncWidth  : 8;
    uint32_t burstWidth  : 8;
    uint32_t vsyncHeight : 4;
    uint32_t burstStart  : 10;
    uint32_t             : 2;
};

struct VI_V_TOTAL {
    uint32_t vTotal : 10;
    uint32_t        : 22;
};

struct VI_H_TOTAL {
    uint32_t hTotal : 12;
    uint32_t        : 4;
    uint32_t leap   : 5;
    uint32_t        : 11;
};

struct VI_H_TOTAL_LEAP {
    uint32_t leapB : 12;
    uint32_t       : 4;
    uint32_t leapA : 12;
    uint32_t       : 4;
};

struct VI_H_VIDEO {
    uint32_t hEnd   : 10;
    uint32_t        : 6;
    uint32_t hStart : 10;
    uint32_t        : 6;
};

struct VI_V_VIDEO {
    uint32_t vEnd   : 10;
    uint32_t        : 6;
    uint32_t vStart : 10;
    uint32_t        : 6;
};

struct VI_V_BURST {
    uint32_t vBurstEnd   : 10;
    uint32_t             : 6;
    uint32_t vBurstStart : 10;
    uint32_t             : 6;
};

struct VI_X_SCALE {
    uint32_t xScale  : 12;
    uint32_t         : 4;
    uint32_t xOffset : 12;
    uint32_t         : 4;
};

struct VI_Y_SCALE {
    uint32_t yScale  : 12;
    uint32_t         : 4;
    uint32_t yOffset : 10;
    uint32_t         : 6;
};

struct VI_TEST_ADDR {
    uint32_t testAddr : 7;
    uint32_t          : 25;
};

struct VI_STAGED_DATA {
    uint32_t stagedData : 32;
};

} // namespace Interfaces