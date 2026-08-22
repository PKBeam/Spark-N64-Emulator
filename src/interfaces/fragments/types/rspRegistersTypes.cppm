export module Interfaces:RspRegistersTypes;

import std;
import Util;
import :Interface;

export namespace Interfaces {

struct RSP_REG_ADDR {
    constexpr static uint32_t BASE = 0x04040000;
    constexpr static uint32_t END  = 0x040FFFFF;
    enum Address : uint32_t {
        RSP_DMA_SPADDR  = 0x04040000,
        RSP_DMA_RAMADDR = 0x04040004,
        RSP_DMA_RDLEN   = 0x04040008,
        RSP_DMA_WRLEN   = 0x0404000C,
        RSP_STATUS      = 0x04040010,
        RSP_DMA_FULL    = 0x04040014,
        RSP_DMA_BUSY    = 0x04040018,
        RSP_SEMAPHORE   = 0x0404001C,
        RSP_PC          = 0x04080000,
    };
};

struct RSP_DMA_SPADDR {
    enum class MEM_BANK : bool {
        DMEM = 0,
        IMEM = 1,
    };
    uint32_t              : 3;
    uint32_t memAddr_11_3 : 9;
    uint32_t memBank      : 1;
    uint32_t              : 19;
};

struct RSP_DMA_RAMADDR {
    uint32_t               : 3;
    uint32_t dramAddr_23_3 : 21;
    uint32_t               : 8;
};

struct RSP_DMA_RDLEN {
    uint32_t rdlen     : 12;
    uint32_t count     : 8;
    uint32_t           : 3;
    uint32_t skip_11_3 : 9;
};

struct RSP_DMA_WRLEN {
    uint32_t wrlen     : 12;
    uint32_t count     : 8;
    uint32_t           : 3;
    uint32_t skip_11_3 : 9;
};

struct RSP_STATUS {
    uint32_t halted   : 1;
    uint32_t broke    : 1;
    uint32_t dmaBusy  : 1;
    uint32_t dmaFull  : 1;
    uint32_t ioBusy   : 1;
    uint32_t sstep    : 1;
    uint32_t intbreak : 1;
    uint32_t sig0     : 1;
    uint32_t sig1     : 1;
    uint32_t sig2     : 1;
    uint32_t sig3     : 1;
    uint32_t sig4     : 1;
    uint32_t sig5     : 1;
    uint32_t sig6     : 1;
    uint32_t sig7     : 1;
    uint32_t          : 17;

    struct Write {
        uint32_t clrHalt     : 1;
        uint32_t setHalt     : 1;
        uint32_t clrBroke    : 1;
        uint32_t clrIntr     : 1;
        uint32_t setIntr     : 1;
        uint32_t clrSstep    : 1;
        uint32_t setSstep    : 1;
        uint32_t clrIntbreak : 1;
        uint32_t setIntbreak : 1;
        uint32_t clrSig0     : 1;
        uint32_t setSig0     : 1;
        uint32_t clrSig1     : 1;
        uint32_t setSig1     : 1;
        uint32_t clrSig2     : 1;
        uint32_t setSig2     : 1;
        uint32_t clrSig3     : 1;
        uint32_t setSig3     : 1;
        uint32_t clrSig4     : 1;
        uint32_t setSig4     : 1;
        uint32_t clrSig5     : 1;
        uint32_t setSig5     : 1;
        uint32_t clrSig6     : 1;
        uint32_t setSig6     : 1;
        uint32_t clrSig7     : 1;
        uint32_t setSig7     : 1;
        uint32_t             : 7;
    };
};

struct RSP_DMA_FULL {
    uint32_t dmaFull : 1;
    uint32_t         : 31;
};

struct RSP_DMA_BUSY {
    uint32_t dmaBusy : 1;
    uint32_t         : 31;
};

struct RSP_SEMAPHORE {
    uint32_t semaphore : 1;
    uint32_t           : 31;
};

struct RSP_PC {
    uint32_t         : 2;
    uint32_t pc_11_2 : 10;
    uint32_t         : 20;
};

} // namespace Interfaces