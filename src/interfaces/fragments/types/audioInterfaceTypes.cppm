export module Interfaces:AudioInterfaceTypes;

import std;
import Util;

import :Interface;

export namespace Interfaces {

struct AI_REG_ADDR {
    static constexpr uint32_t BASE = 0x04500000;
    static constexpr uint32_t END  = 0x045FFFFF;
    enum Address : uint32_t {
        AI_DRAM_ADDR = 0x04500000,
        AI_LENGTH    = 0x04500004,
        AI_CONTROL   = 0x04500008,
        AI_STATUS    = 0x0450000C,
        AI_DACRATE   = 0x04500010,
        AI_BITRATE   = 0x04500014,
    };
};

struct AI_DRAM_ADDR {
    uint32_t               : 3;
    uint32_t dramAddr_23_3 : 21;
    uint32_t               : 8;
};

struct AI_LENGTH {
    uint32_t             : 3;
    uint32_t length_17_3 : 15;
    uint32_t             : 14;
};

struct AI_CONTROL {
    uint32_t dmaEnable : 1;
    uint32_t           : 31;
};

struct AI_STATUS {
    uint32_t full0   : 1;
    uint32_t count   : 14;
    uint32_t         : 1;
    uint32_t bc      : 1;
    uint32_t         : 2;
    uint32_t wc      : 1;
    uint32_t         : 5;
    uint32_t enabled : 1;
    uint32_t         : 4;
    uint32_t full1   : 1;
    uint32_t busy    : 1;
};

struct AI_DACRATE {
    uint32_t dacRate : 14;
    uint32_t         : 18;
};

struct AI_BITRATE {
    uint32_t bitRate : 4;
    uint32_t         : 28;
};

} // namespace Interfaces