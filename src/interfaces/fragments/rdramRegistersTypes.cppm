export module Interfaces:RdramRegistersTypes;

import std;
import Util;
import :MmioRegisters;

namespace Interfaces {

struct rdramRegs;

export struct RDRAM_REG_ADDR {
    constexpr static uint32_t BASE = 0x03F00000;
    constexpr static uint32_t END  = 0x03FFFFFF;
    enum {
        MI_MODE_OFFSET      = 0x0,
        MI_VERSION_OFFSET   = 0x4,
        MI_INTERRUPT_OFFSET = 0x8,
        MI_MASK_OFFSET      = 0xC,
    };
};

export struct RDRAM_REG_DEVICE_TYPE {
    uint32_t type       : 4;
    uint32_t version    : 4;
    uint32_t            : 8;
    uint32_t rowBits    : 4;
    uint32_t bankBits   : 4;
    uint32_t en         : 1;
    uint32_t            : 1;
    uint32_t bn         : 1;
    uint32_t            : 1;
    uint32_t columnBits : 4;
};

export struct RDRAM_REG_DEVICE_ID : WriteableRegister {
    uint32_t               : 7;
    uint32_t idField_35    : 1;
    uint32_t idField_34_27 : 8;
    uint32_t               : 7;
    uint32_t idField_26    : 1;
    uint32_t               : 2;
    uint32_t idField_25_20 : 6;
};

export struct RDRAM_REG_DELAY : WriteableRegister {
    uint32_t writeBits   : 3;
    uint32_t writeDelay  : 3;
    uint32_t             : 2;
    uint32_t ackBits     : 3;
    uint32_t ackDelay    : 2;
    uint32_t             : 3;
    uint32_t readBits    : 3;
    uint32_t readDelay   : 3;
    uint32_t             : 2;
    uint32_t ackWinBits  : 3;
    uint32_t ackWinDelay : 3;
    uint32_t             : 2;
};

export struct RDRAM_REG_MODE : WriteableRegister {
    uint32_t    : 6;
    uint32_t c0 : 1;
    uint32_t c3 : 1;
    uint32_t    : 6;
    uint32_t c1 : 1;
    uint32_t c4 : 1;
    uint32_t    : 3;
    uint32_t ad : 1;
    uint32_t    : 2;
    uint32_t c2 : 1;
    uint32_t c5 : 1;
    uint32_t le : 1;
    uint32_t de : 1;
    uint32_t as : 1;
    uint32_t sk : 1;
    uint32_t sv : 1;
    uint32_t pl : 1;
    uint32_t x2 : 1;
    uint32_t ce : 1;
};

export struct RDRAM_REG_REF_INTERVAL {
    uint32_t : 32;
};

export struct RDRAM_REG_REF_ROW : WriteableRegister {
    uint32_t              : 8;
    uint32_t rowField_9_8 : 2;
    uint32_t              : 9;
    uint32_t bankField    : 1;
    uint32_t              : 4;
    uint32_t              : 1;
    uint32_t rowField_7_1 : 7;
};

export struct RDRAM_REG_RAS_INTERVAL : WriteableRegister {
    uint32_t rowExpRestore : 5;
    uint32_t               : 3;
    uint32_t rowImpRestore : 5;
    uint32_t               : 3;
    uint32_t rowSense      : 5;
    uint32_t               : 3;
    uint32_t rowPrecharge  : 5;
    uint32_t               : 3;
};

export struct RDRAM_REG_MIN_INTERVAL : WriteableRegister {
    uint32_t specFunc : 5;
    uint32_t mwd_0    : 1;
    uint32_t mrd_0    : 1;
    uint32_t mad_0    : 1;
    uint32_t          : 5;
    uint32_t mwd_1    : 1;
    uint32_t mrd_1    : 1;
    uint32_t mad_1    : 1;
    uint32_t          : 5;
    uint32_t mwd_2    : 1;
    uint32_t mrd_2    : 1;
    uint32_t mad_2    : 1;
    uint32_t          : 5;
    uint32_t mwd_3    : 1;
    uint32_t mrd_3    : 1;
    uint32_t mad_3    : 1;
};

export struct RDRAM_REG_ADDRESS_SELECT : WriteableRegister {
    uint32_t               : 16;
    uint32_t swapField_8_7 : 2;
    uint32_t               : 7;
    uint32_t swapField_6_0 : 7;
};

export struct RDRAM_REG_DEVICE_MANUFACTURER {
    uint32_t manufacture_15_8     : 8;
    uint32_t manufacture_7_0      : 8;
    uint32_t manufactureCode_15_8 : 8;
    uint32_t manufactureCode_7_0  : 8;
};

struct rdramRegs {
    RDRAM_REG_DEVICE_TYPE         deviceType;
    RDRAM_REG_DEVICE_ID           deviceId;
    RDRAM_REG_DELAY               delay;
    RDRAM_REG_MODE                mode;
    RDRAM_REG_REF_INTERVAL        refInterval;
    RDRAM_REG_REF_ROW             refRow;
    RDRAM_REG_RAS_INTERVAL        rasInterval;
    RDRAM_REG_MIN_INTERVAL        minInterval;
    RDRAM_REG_ADDRESS_SELECT      addressSelect;
    RDRAM_REG_DEVICE_MANUFACTURER deviceManufacturer;
};

} // namespace Interfaces