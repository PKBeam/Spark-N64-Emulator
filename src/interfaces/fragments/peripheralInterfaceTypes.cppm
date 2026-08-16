export module Interfaces:PeripheralInterfaceTypes;

import std;
import Util;

import :MmioRegisters;

namespace Interfaces {

export {

    enum class PiDmaRanges {
        // clang-format off
        RDRAM          [[=Util::Range{0x00000000, 0x03EFFFFF}]],
        PI_REG         [[=Util::Range{0x04600000, 0x046FFFFF}]],
        N64DD_CTRL_REG [[=Util::Range{0x05000000, 0x05FFFFFF}]],
        N64DD_IPL_ROM  [[=Util::Range{0x06000000, 0x07FFFFFF}]],
        SRAM           [[=Util::Range{0x08000000, 0x0FFFFFFF}]],
        ROM            [[=Util::Range{0x10000000, 0x1FBFFFFF}]],
        // clang-format on
    };

    struct PI_REG_ADDR {
        constexpr static uint32_t BASE = 0x04600000;
        constexpr static uint32_t END  = 0x046FFFFF;
        enum {
            PI_DRAM_ADDR    = 0x04600000,
            PI_CART_ADDR    = 0x04600004,
            PI_RD_LEN       = 0x04600008,
            PI_WR_LEN       = 0x0460000C,
            PI_STATUS       = 0x04600010,
            PI_BSD_DOM1_LAT = 0x04600014,
            PI_BSD_DOM1_PWD = 0x04600018,
            PI_BSD_DOM1_PGS = 0x0460001C,
            PI_BSD_DOM1_RLS = 0x04600020,
            PI_BSD_DOM2_LAT = 0x04600024,
            PI_BSD_DOM2_PWD = 0x04600028,
            PI_BSD_DOM2_PGS = 0x0460002C,
            PI_BSD_DOM2_RLS = 0x04600030,
        };
    };

    struct PI_DRAM_ADDR : WriteableRegister {
        uint32_t               : 1;
        uint32_t dramAddr_23_1 : 23;
        uint32_t               : 8;

        auto write(uint32_t bits) -> void;
    };

    struct PI_CART_ADDR : WriteableRegister {
        uint32_t               : 1;
        uint32_t cartAddr_31_1 : 31;

        auto write(uint32_t bits) -> void;
    };

    struct PI_RD_LEN : WriteableRegister {
        uint32_t rdLen_23_0 : 24;
        uint32_t            : 8;

        auto write(uint32_t bits) -> void;
    };

    struct PI_WR_LEN : WriteableRegister {
        uint32_t wrLen_23_0 : 24;
        uint32_t            : 8;

        auto write(uint32_t bits) -> void;
    };

    struct PI_STATUS : WriteableRegister {
        uint32_t dmaBusy   : 1;
        uint32_t ioBusy    : 1;
        uint32_t dmaError  : 1;
        uint32_t interrupt : 1;
        uint32_t           : 28;

        struct PI_STATUS_write {
            uint32_t dmaReset       : 1;
            uint32_t clearInterrupt : 1;
            uint32_t                : 30;
        };

        auto write(uint32_t bits) -> void;
    };

    struct PI_BSD_DOM_LAT : WriteableRegister {
        uint32_t lat : 8 = 64;
        uint32_t     : 24;

        auto write(uint32_t bits) -> void;
    };

    struct PI_BSD_DOM1_LAT : PI_BSD_DOM_LAT {};
    struct PI_BSD_DOM2_LAT : PI_BSD_DOM_LAT {};

    struct PI_BSD_DOM_PWD : WriteableRegister {
        uint32_t pwd : 8 = 18;
        uint32_t     : 24;

        auto write(uint32_t bits) -> void;
    };

    struct PI_BSD_DOM1_PWD : PI_BSD_DOM_PWD {};
    struct PI_BSD_DOM2_PWD : PI_BSD_DOM_PWD {};

    struct PI_BSD_DOM_PGS : WriteableRegister {
        uint32_t pgs : 4 = 7;
        uint32_t     : 28;

        auto write(uint32_t bits) -> void;
    };

    struct PI_BSD_DOM1_PGS : PI_BSD_DOM_PGS {};
    struct PI_BSD_DOM2_PGS : PI_BSD_DOM_PGS {};

    struct PI_BSD_DOM_RLS : WriteableRegister {
        uint32_t rls : 2 = 3;
        uint32_t     : 30;

        auto write(uint32_t bits) -> void;
    };

    struct PI_BSD_DOM1_RLS : PI_BSD_DOM_RLS {};
    struct PI_BSD_DOM2_RLS : PI_BSD_DOM_RLS {};

    struct piRegs {
        PI_DRAM_ADDR    dramAddr;
        PI_CART_ADDR    cartAddr;
        PI_RD_LEN       rdLen;
        PI_WR_LEN       wrLen;
        PI_STATUS       status;
        PI_BSD_DOM1_LAT bsdDom1Lat;
        PI_BSD_DOM1_PWD bsdDom1Pwd;
        PI_BSD_DOM1_PGS bsdDom1Pgs;
        PI_BSD_DOM1_RLS bsdDom1Rls;
        PI_BSD_DOM2_LAT bsdDom2Lat;
        PI_BSD_DOM2_PWD bsdDom2Pwd;
        PI_BSD_DOM2_PGS bsdDom2Pgs;
        PI_BSD_DOM2_RLS bsdDom2Rls;
    };
}

} // namespace Interfaces