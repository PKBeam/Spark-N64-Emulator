export module Interfaces:RdramInterfaceTypes;

import std;
import Util;

import :MmioRegisters;

namespace Interfaces {

struct riRegs;

export struct RI_REG_ADDR {
    constexpr static uint32_t BASE = 0x04700000;
    constexpr static uint32_t END  = 0x047FFFFF;
    enum {
        RI_MODE_OFFSET         = 0x00,
        RI_CONFIG_OFFSET       = 0x04,
        RI_CURRENT_LOAD_OFFSET = 0x08,
        RI_SELECT_OFFSET       = 0x0C,
        RI_REFRESH_OFFSET      = 0x10,
        RI_LATENCY_OFFSET      = 0x14,
        RI_ERROR_OFFSET        = 0x18,
        RI_BANK_STATUS_OFFSET  = 0x1C,
    };
};

export struct RI_MODE : WriteableRegister {
    uint32_t opMode : 2 = 0x2;
    uint32_t stopT  : 1 = 0x1;
    uint32_t stopR  : 1 = 0x1;
    uint32_t        : 28;

    auto write(uint32_t bits) -> void;
};

export struct RI_CONFIG : WriteableRegister {
    uint32_t cc     : 6;
    uint32_t autoCc : 1;
    uint32_t        : 25;

    auto write(uint32_t bits) -> void;
};

export struct RI_CURRENT_LOAD : WriteableRegister {
    uint32_t : 32;

    auto write(uint32_t bits) -> void;
};

export struct RI_SELECT : WriteableRegister {
    uint32_t rSel : 4;
    uint32_t tSel : 4;
    uint32_t      : 24;

    auto write(uint32_t bits) -> void;
};

export struct RI_REFRESH : WriteableRegister {
    uint32_t cleanRefreshDelay : 8;
    uint32_t dirtyRefreshDelay : 8;
    uint32_t bank              : 1;
    uint32_t en                : 1;
    uint32_t opt               : 1;
    uint32_t multiBank         : 4;
    uint32_t                   : 9;

    auto write(uint32_t bits) -> void;
};

export struct RI_LATENCY : WriteableRegister {
    uint32_t dmaLatencyOverlap : 5;
    uint32_t                   : 27;

    auto write(uint32_t bits) -> void;
};

export struct RI_ERROR : WriteableRegister {
    uint32_t ack  : 1;
    uint32_t nack : 1;
    uint32_t over : 1;
    uint32_t      : 29;

    auto write(uint32_t bits) -> void;
};

export struct RI_BANK_STATUS : WriteableRegister {
    uint32_t bankDirtyBits : 8;
    uint32_t bankValidBits : 8;
    uint32_t               : 16;

    auto write(uint32_t bits) -> void;
};

struct riRegs {
    RI_MODE         riMode;
    RI_CONFIG       riConfig;
    RI_CURRENT_LOAD riCurrentLoad;
    RI_SELECT       riSelect;
    RI_REFRESH      riRefresh;
    RI_LATENCY      riLatency;
    RI_ERROR        riError;
    RI_BANK_STATUS  riBankStatus;
};

auto RI_MODE::write(uint32_t bits) -> void {
    auto wr = std::bit_cast<RI_MODE>(bits);
    opMode  = wr.opMode;
    stopT   = wr.stopT;
    stopR   = wr.stopR;
}

auto RI_CONFIG::write(uint32_t bits) -> void {
    auto wr = std::bit_cast<RI_CONFIG>(bits);
    cc      = wr.cc;
    autoCc  = wr.autoCc;
}

auto RI_CURRENT_LOAD::write(uint32_t bits) -> void {
    // Any write to this register causes a new value to be loaded into the RAC current control register
}

auto RI_SELECT::write(uint32_t bits) -> void {
    auto wr = std::bit_cast<RI_SELECT>(bits);
    rSel    = wr.rSel;
    tSel    = wr.tSel;
}

auto RI_REFRESH::write(uint32_t bits) -> void {
    auto wr           = std::bit_cast<RI_REFRESH>(bits);
    cleanRefreshDelay = wr.cleanRefreshDelay;
    dirtyRefreshDelay = wr.dirtyRefreshDelay;
    bank              = wr.bank;
    en                = wr.en;
    opt               = wr.opt;
    multiBank         = wr.multiBank;
}

auto RI_LATENCY::write(uint32_t bits) -> void {
    auto wr           = std::bit_cast<RI_LATENCY>(bits);
    dmaLatencyOverlap = wr.dmaLatencyOverlap;
}

auto RI_ERROR::write(uint32_t bits) -> void {
    ack  = 0;
    nack = 0;
    over = 0;
}

auto RI_BANK_STATUS::write(uint32_t bits) -> void {
    bankDirtyBits = 0xFF;
    bankValidBits = 0;
}

} // namespace Interfaces