export module Interfaces:RdramInterfaceTypes;

import std;
import Util;

import :Interface;

export namespace Interfaces {

struct RI_REG_ADDR {
    constexpr static uint32_t BASE = 0x04700000;
    constexpr static uint32_t END  = 0x047FFFFF;
    enum Address : uint32_t {
        RI_MODE         = 0x04700000,
        RI_CONFIG       = 0x04700004,
        RI_CURRENT_LOAD = 0x04700008,
        RI_SELECT       = 0x0470000C,
        RI_REFRESH      = 0x04700010,
        RI_LATENCY      = 0x04700014,
        RI_ERROR        = 0x04700018,
        RI_BANK_STATUS  = 0x0470001C,
    };
};

struct RI_MODE {
    uint32_t opMode : 2 = 0x2;
    uint32_t stopT  : 1 = 0x1;
    uint32_t stopR  : 1 = 0x1;
    uint32_t        : 28;
};

struct RI_CONFIG {
    uint32_t cc     : 6;
    uint32_t autoCc : 1;
    uint32_t        : 25;
};

struct RI_CURRENT_LOAD {
    uint32_t : 32;
};

struct RI_SELECT {
    uint32_t rSel : 4;
    uint32_t tSel : 4;
    uint32_t      : 24;
};

struct RI_REFRESH {
    uint32_t cleanRefreshDelay : 8;
    uint32_t dirtyRefreshDelay : 8;
    uint32_t bank              : 1;
    uint32_t en                : 1;
    uint32_t opt               : 1;
    uint32_t multiBank         : 4;
    uint32_t                   : 9;
};

struct RI_LATENCY {
    uint32_t dmaLatencyOverlap : 5;
    uint32_t                   : 27;
};

struct RI_ERROR {
    uint32_t ack  : 1;
    uint32_t nack : 1;
    uint32_t over : 1;
    uint32_t      : 29;
};

struct RI_BANK_STATUS {
    uint32_t bankDirtyBits : 8;
    uint32_t bankValidBits : 8;
    uint32_t               : 16;
};

} // namespace Interfaces