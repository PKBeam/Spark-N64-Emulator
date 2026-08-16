export module Interfaces:SerialInterfaceTypes;

import std;
import Util;

import :MmioRegisters;

namespace Interfaces {

export {

    enum class SiDmaRanges {
        // clang-format off
        PIF_ROM [[=Util::Range{0x1FC00000, 0x1FC007BF}]],
        PIF_RAM [[=Util::Range{0x1FC007C0, 0x1FC007FF}]],
        // clang-format on
    };

    struct SI_REG_ADDR {
        constexpr static uint32_t BASE = 0x04800000;
        constexpr static uint32_t END  = 0x048FFFFF;
        enum {
            SI_DRAM_ADDR    = 0x04800000,
            SI_PIF_AD_RD64B = 0x04800004,
            SI_PIF_AD_WR4B  = 0x04800008,
            SI_PIF_AD_WR64B = 0x04800010,
            SI_PIF_AD_RD4B  = 0x04800014,
            SI_STATUS       = 0x04800018,
        };
    };

    struct SI_DRAM_ADDR {
        uint32_t dramAddr_23_0 : 24;
        uint32_t               : 8;
    };

    struct SI_PIF_AD_RD64B {
        uint32_t              : 2;
        uint32_t pifAddr_10_2 : 9;
        uint32_t              : 21;
    };

    struct SI_PIF_AD_WR4B {
        uint32_t data : 32;
    };

    struct SI_PIF_AD_WR64B {
        uint32_t : 32;
    };

    struct SI_PIF_AD_RD4B {
        uint32_t data : 32;
    };

    struct SI_STATUS {
        uint32_t dmaBusy     : 1;
        uint32_t ioBusy      : 1;
        uint32_t readPending : 1;
        uint32_t dmaError    : 1;
        uint32_t pchState    : 4;
        uint32_t dmaState    : 4;
        uint32_t interrupt   : 1;
        uint32_t             : 19;
    };
}

} // namespace Interfaces