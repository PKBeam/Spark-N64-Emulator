module;

#include <util/defines.hpp>

export module Rom:Types;

import std;
import Util;

// rules:
// - no bitfields allowed with types larger than 1 byte
// - only use integral types for fields that should be interpreted as integers, otherwise use std::byte[]

export STRUCT_PACKED(PI_BSD_DOM1) {
    uint8_t PGS : 4 = 0x7;
    uint8_t RLS : 4 = 0x3;
    uint8_t PWD     = 0x12;
    uint8_t LAT     = 0x40;
};
static_assert(sizeof(PI_BSD_DOM1) == 3);

// TODO reflection helper to automatically byteswap fields when serialising from ROM data
export STRUCT_PACKED(N64RomHeader) {
    std::byte   reserved_0;
    PI_BSD_DOM1 piBsdDom1Flags;
    uint32_t    clockRate;
    uint32_t    bootAddress;
    uint32_t    libultraVersion;
    uint64_t    checkCode;
    std::byte   reserved_1[8];
    char        gameTitle[20];
    std::byte   reserved_2[7];
    uint32_t    gameCode;
    uint8_t     romVersion;
};
static_assert(sizeof(N64RomHeader) == 64);

constexpr std::size_t IPL3_CODE_OFFSET = 0x40;
constexpr std::size_t ROM_CODE_OFFSET  = 0x1000;
