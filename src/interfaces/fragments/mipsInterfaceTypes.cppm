export module Interfaces:MipsInterfaceTypes;

import std;
import Util;
import :MmioRegisters;

namespace Interfaces {

struct miRegs;

export struct MI_REG_ADDR {
    constexpr static uint32_t BASE = 0x04300000;
    constexpr static uint32_t END  = 0x043FFFFF;
    enum {
        MI_MODE_OFFSET      = 0x0,
        MI_VERSION_OFFSET   = 0x4,
        MI_INTERRUPT_OFFSET = 0x8,
        MI_MASK_OFFSET      = 0xC,
    };
};

export struct MI_MODE : WriteableRegister {
    uint32_t repeatCount : 7;
    uint32_t repeat      : 1;
    uint32_t eBus        : 1;
    uint32_t upper       : 1;
    uint32_t             : 22;

    struct MI_MODE_write {
        uint32_t repeatCount : 7;
        uint32_t clearRepeat : 1;
        uint32_t setRepeat   : 1;
        uint32_t clearEBus   : 1;
        uint32_t setEBus     : 1;
        uint32_t clearDp     : 1;
        uint32_t clearUpper  : 1;
        uint32_t setUpper    : 1;
        uint32_t             : 18;
    };

    auto write(uint32_t bits) -> void;
};

export struct MI_VERSION {
    uint32_t value = 0x02020102;
};

export struct MI_INTERRUPT {
    uint32_t sp : 1;
    uint32_t si : 1;
    uint32_t ai : 1;
    uint32_t vi : 1;
    uint32_t pi : 1;
    uint32_t dp : 1;
    uint32_t    : 26;
};

export struct MI_MASK : WriteableRegister {
    uint32_t sp : 1;
    uint32_t si : 1;
    uint32_t ai : 1;
    uint32_t vi : 1;
    uint32_t pi : 1;
    uint32_t dp : 1;
    uint32_t    : 26;

    struct MI_MASK_write {
        uint32_t clearSp : 1;
        uint32_t setSp   : 1;
        uint32_t clearSi : 1;
        uint32_t setSi   : 1;
        uint32_t clearAi : 1;
        uint32_t setAi   : 1;
        uint32_t clearVi : 1;
        uint32_t setVi   : 1;
        uint32_t clearPi : 1;
        uint32_t setPi   : 1;
        uint32_t clearDp : 1;
        uint32_t setDp   : 1;
        uint32_t         : 20;
    };

    auto write(uint32_t bits) -> void;
};

struct miRegs {
    MI_MODE      miMode;
    MI_VERSION   miVersion;
    MI_INTERRUPT miInterrupt;
    MI_MASK      miMask;
};

auto MI_MODE::write(uint32_t bits) -> void {
    auto wr = std::bit_cast<MI_MODE_write>(bits);
    if (wr.setRepeat) {
        repeat      = 1;
        repeatCount = wr.repeatCount;
    }
    if (wr.clearRepeat) repeat = 0;
    if (wr.setEBus) eBus = 1;
    if (wr.clearDp) {
        // handled by mipsInterface
    }
    if (wr.setUpper) upper = 1;
    if (wr.clearUpper) upper = 0;
}

auto MI_MASK::write(uint32_t bits) -> void {
    auto wr = std::bit_cast<MI_MASK_write>(bits);
    if (wr.clearSp) sp = 0;
    if (wr.setSp) sp = 1;
    if (wr.clearSi) si = 0;
    if (wr.setSi) si = 1;
    if (wr.clearAi) ai = 0;
    if (wr.setAi) ai = 1;
    if (wr.clearVi) vi = 0;
    if (wr.setVi) vi = 1;
    if (wr.clearPi) pi = 0;
    if (wr.setPi) pi = 1;
    if (wr.clearDp) dp = 0;
    if (wr.setDp) dp = 1;
}

} // namespace Interfaces