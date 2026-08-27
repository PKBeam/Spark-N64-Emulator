export module InterfaceTypes:RdpRegistersTypes;

import std;
import Util;

export namespace Interfaces {

struct RDP_REG_ADDR {
    constexpr static uint32_t BASE = 0x04100000;
    constexpr static uint32_t END  = 0x041FFFFF;
    enum Address : uint32_t {
        DPC_START        = 0x04100000,
        DPC_END          = 0x04100004,
        DPC_CURRENT      = 0x04100008,
        DPC_STATUS       = 0x0410000C,
        DPC_CLOCK        = 0x04100010,
        DPC_CMD_BUSY     = 0x04100014,
        DPC_PIPE_BUSY    = 0x04100018,
        DPC_TMEM_BUSY    = 0x0410001C,
        DPS_TBIST        = 0x04200000,
        DPS_TEST_MODE    = 0x04200004,
        DPS_BUFTEST_ADDR = 0x04200008,
        DPS_BUFTEST_DATA = 0x0420000C,
    };
};

struct DPC_START {
    uint32_t            : 3;
    uint32_t start_24_3 : 21;
    uint32_t            : 8;
};

struct DPC_END {
    uint32_t          : 3;
    uint32_t end_24_3 : 21;
    uint32_t          : 8;
};

struct DPC_CURRENT {
    uint32_t current : 24;
    uint32_t         : 8;
};

struct DPC_STATUS {
    uint32_t xbus         : 1;
    uint32_t freeze       : 1;
    uint32_t flush        : 1;
    uint32_t gclk         : 1;
    uint32_t tmemBusy     : 1;
    uint32_t pipeBusy     : 1;
    uint32_t cmdBusy      : 1;
    uint32_t cbufReady    : 1;
    uint32_t dmaBusy      : 1;
    uint32_t endPending   : 1;
    uint32_t startPending : 1;
    uint32_t              : 21;

    struct Write {
        uint32_t clrXbus       : 1;
        uint32_t setXbus       : 1;
        uint32_t clrFreeze     : 1;
        uint32_t setFreeze     : 1;
        uint32_t clrFlush      : 1;
        uint32_t setFlush      : 1;
        uint32_t clrTmemBusy   : 1;
        uint32_t clrPipeBusy   : 1;
        uint32_t clrBufferBusy : 1;
        uint32_t clrClock      : 1;
        uint32_t               : 22;
    };
};

struct DPC_CLOCK {
    uint32_t clock : 24;
    uint32_t       : 8;
};

struct DPC_CMD_BUSY {
    uint32_t cmdBusy : 24;
    uint32_t         : 8;
};

struct DPC_PIPE_BUSY {
    uint32_t pipeBusy : 24;
    uint32_t          : 8;
};

struct DPC_TMEM_BUSY {
    uint32_t tmemBusy : 24;
    uint32_t          : 8;
};

struct DPS_TBIST {
    uint32_t check : 1;
    uint32_t go    : 1;
    uint32_t done  : 1;
    uint32_t fail  : 8;
    uint32_t       : 21;
};

struct DPS_TEST_MODE {
    uint32_t testEnable : 1 = 1;
    uint32_t            : 1;
    uint32_t _          : 1 = 1;
    uint32_t            : 4;
    uint32_t _          : 1 = 1;
    uint32_t            : 4;
    uint32_t zspan1     : 4;
    uint32_t zspan0     : 4;
    uint32_t cspan1     : 4;
    uint32_t cspan0     : 4;
    uint32_t            : 4;

    struct Write {
        uint32_t testEnable : 1 = 1;
        uint32_t            : 31;
    };
};

struct DPS_BUFTEST_ADDR {
    uint32_t address : 7;
    uint32_t         : 25;
};

struct DPS_BUFTEST_DATA {
    uint32_t data : 32;
};
} // namespace Interfaces