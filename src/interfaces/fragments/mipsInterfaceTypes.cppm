export module Interfaces:MipsInterfaceTypes;

import std;
import Util;
import :MmioRegisters;

namespace Interfaces {

export {
    struct MI_REG_ADDR {
        constexpr static uint32_t BASE = 0x04300000;
        constexpr static uint32_t END  = 0x043FFFFF;
        enum {
            MI_MODE      = 0x04300000,
            MI_VERSION   = 0x04300004,
            MI_INTERRUPT = 0x04300008,
            MI_MASK      = 0x0430000C,
        };
    };

    struct MI_MODE {
        uint32_t repeatCount : 7;
        uint32_t repeat      : 1;
        uint32_t eBus        : 1;
        uint32_t upper       : 1;
        uint32_t             : 22;

        struct Write {
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
    };

    struct MI_VERSION {
        uint32_t value = 0x02020102;
    };

    struct MI_INTERRUPT {
        uint32_t sp : 1;
        uint32_t si : 1;
        uint32_t ai : 1;
        uint32_t vi : 1;
        uint32_t pi : 1;
        uint32_t dp : 1;
        uint32_t    : 26;
    };

    struct MI_MASK {
        uint32_t sp : 1;
        uint32_t si : 1;
        uint32_t ai : 1;
        uint32_t vi : 1;
        uint32_t pi : 1;
        uint32_t dp : 1;
        uint32_t    : 26;

        struct Write {
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
    };
}

} // namespace Interfaces