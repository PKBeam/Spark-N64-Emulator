module;

#include <util/defines.hpp>

export module RSP:InstructionExecutor;

import std;
import CPU;
import ISA;
import Memory;
import MemoryTypes;
import Util;

import :Registers;

constexpr auto RCP_LOOKUP = std::array<uint16_t, 512>{
    // clang-format off
    0xffff, 0xff00, 0xfe01, 0xfd04, 0xfc07, 0xfb0c, 0xfa11, 0xf918, 0xf81f, 0xf727, 0xf631, 0xf53b, 0xf446, 0xf352, 0xf25f, 0xf16d,
    0xf07c, 0xef8b, 0xee9c, 0xedae, 0xecc0, 0xebd3, 0xeae8, 0xe9fd, 0xe913, 0xe829, 0xe741, 0xe65a, 0xe573, 0xe48d, 0xe3a9, 0xe2c5,
    0xe1e1, 0xe0ff, 0xe01e, 0xdf3d, 0xde5d, 0xdd7e, 0xdca0, 0xdbc2, 0xdae6, 0xda0a, 0xd92f, 0xd854, 0xd77b, 0xd6a2, 0xd5ca, 0xd4f3,
    0xd41d, 0xd347, 0xd272, 0xd19e, 0xd0cb, 0xcff8, 0xcf26, 0xce55, 0xcd85, 0xccb5, 0xcbe6, 0xcb18, 0xca4b, 0xc97e, 0xc8b2, 0xc7e7,
    0xc71c, 0xc652, 0xc589, 0xc4c0, 0xc3f8, 0xc331, 0xc26b, 0xc1a5, 0xc0e0, 0xc01c, 0xbf58, 0xbe95, 0xbdd2, 0xbd10, 0xbc4f, 0xbb8f,
    0xbacf, 0xba10, 0xb951, 0xb894, 0xb7d6, 0xb71a, 0xb65e, 0xb5a2, 0xb4e8, 0xb42e, 0xb374, 0xb2bb, 0xb203, 0xb14b, 0xb094, 0xafde,
    0xaf28, 0xae73, 0xadbe, 0xad0a, 0xac57, 0xaba4, 0xaaf1, 0xaa40, 0xa98e, 0xa8de, 0xa82e, 0xa77e, 0xa6d0, 0xa621, 0xa574, 0xa4c6,
    0xa41a, 0xa36e, 0xa2c2, 0xa217, 0xa16d, 0xa0c3, 0xa01a, 0x9f71, 0x9ec8, 0x9e21, 0x9d79, 0x9cd3, 0x9c2d, 0x9b87, 0x9ae2, 0x9a3d,
    0x9999, 0x98f6, 0x9852, 0x97b0, 0x970e, 0x966c, 0x95cb, 0x952b, 0x948b, 0x93eb, 0x934c, 0x92ad, 0x920f, 0x9172, 0x90d4, 0x9038,
    0x8f9c, 0x8f00, 0x8e65, 0x8dca, 0x8d30, 0x8c96, 0x8bfc, 0x8b64, 0x8acb, 0x8a33, 0x899c, 0x8904, 0x886e, 0x87d8, 0x8742, 0x86ad,
    0x8618, 0x8583, 0x84f0, 0x845c, 0x83c9, 0x8336, 0x82a4, 0x8212, 0x8181, 0x80f0, 0x8060, 0x7fd0, 0x7f40, 0x7eb1, 0x7e22, 0x7d93,
    0x7d05, 0x7c78, 0x7beb, 0x7b5e, 0x7ad2, 0x7a46, 0x79ba, 0x792f, 0x78a4, 0x781a, 0x7790, 0x7706, 0x767d, 0x75f5, 0x756c, 0x74e4,
    0x745d, 0x73d5, 0x734f, 0x72c8, 0x7242, 0x71bc, 0x7137, 0x70b2, 0x702e, 0x6fa9, 0x6f26, 0x6ea2, 0x6e1f, 0x6d9c, 0x6d1a, 0x6c98,
    0x6c16, 0x6b95, 0x6b14, 0x6a94, 0x6a13, 0x6993, 0x6914, 0x6895, 0x6816, 0x6798, 0x6719, 0x669c, 0x661e, 0x65a1, 0x6524, 0x64a8,
    0x642c, 0x63b0, 0x6335, 0x62ba, 0x623f, 0x61c5, 0x614b, 0x60d1, 0x6058, 0x5fdf, 0x5f66, 0x5eed, 0x5e75, 0x5dfd, 0x5d86, 0x5d0f,
    0x5c98, 0x5c22, 0x5bab, 0x5b35, 0x5ac0, 0x5a4b, 0x59d6, 0x5961, 0x58ed, 0x5879, 0x5805, 0x5791, 0x571e, 0x56ac, 0x5639, 0x55c7,
    0x5555, 0x54e3, 0x5472, 0x5401, 0x5390, 0x5320, 0x52af, 0x5240, 0x51d0, 0x5161, 0x50f2, 0x5083, 0x5015, 0x4fa6, 0x4f38, 0x4ecb,
    0x4e5e, 0x4df1, 0x4d84, 0x4d17, 0x4cab, 0x4c3f, 0x4bd3, 0x4b68, 0x4afd, 0x4a92, 0x4a27, 0x49bd, 0x4953, 0x48e9, 0x4880, 0x4817,
    0x47ae, 0x4745, 0x46dc, 0x4674, 0x460c, 0x45a5, 0x453d, 0x44d6, 0x446f, 0x4408, 0x43a2, 0x433c, 0x42d6, 0x4270, 0x420b, 0x41a6,
    0x4141, 0x40dc, 0x4078, 0x4014, 0x3fb0, 0x3f4c, 0x3ee8, 0x3e85, 0x3e22, 0x3dc0, 0x3d5d, 0x3cfb, 0x3c99, 0x3c37, 0x3bd6, 0x3b74,
    0x3b13, 0x3ab2, 0x3a52, 0x39f1, 0x3991, 0x3931, 0x38d2, 0x3872, 0x3813, 0x37b4, 0x3755, 0x36f7, 0x3698, 0x363a, 0x35dc, 0x357f,
    0x3521, 0x34c4, 0x3467, 0x340a, 0x33ae, 0x3351, 0x32f5, 0x3299, 0x323e, 0x31e2, 0x3187, 0x312c, 0x30d1, 0x3076, 0x301c, 0x2fc2,
    0x2f68, 0x2f0e, 0x2eb4, 0x2e5b, 0x2e02, 0x2da9, 0x2d50, 0x2cf8, 0x2c9f, 0x2c47, 0x2bef, 0x2b97, 0x2b40, 0x2ae8, 0x2a91, 0x2a3a,
    0x29e4, 0x298d, 0x2937, 0x28e0, 0x288b, 0x2835, 0x27df, 0x278a, 0x2735, 0x26e0, 0x268b, 0x2636, 0x25e2, 0x258d, 0x2539, 0x24e5,
    0x2492, 0x243e, 0x23eb, 0x2398, 0x2345, 0x22f2, 0x22a0, 0x224d, 0x21fb, 0x21a9, 0x2157, 0x2105, 0x20b4, 0x2063, 0x2012, 0x1fc1,
    0x1f70, 0x1f1f, 0x1ecf, 0x1e7f, 0x1e2e, 0x1ddf, 0x1d8f, 0x1d3f, 0x1cf0, 0x1ca1, 0x1c52, 0x1c03, 0x1bb4, 0x1b66, 0x1b17, 0x1ac9,
    0x1a7b, 0x1a2d, 0x19e0, 0x1992, 0x1945, 0x18f8, 0x18ab, 0x185e, 0x1811, 0x17c4, 0x1778, 0x172c, 0x16e0, 0x1694, 0x1648, 0x15fd,
    0x15b1, 0x1566, 0x151b, 0x14d0, 0x1485, 0x143b, 0x13f0, 0x13a6, 0x135c, 0x1312, 0x12c8, 0x127f, 0x1235, 0x11ec, 0x11a3, 0x1159,
    0x1111, 0x10c8, 0x107f, 0x1037, 0x0fef, 0x0fa6, 0x0f5e, 0x0f17, 0x0ecf, 0x0e87, 0x0e40, 0x0df9, 0x0db2, 0x0d6b, 0x0d24, 0x0cdd,
    0x0c97, 0x0c50, 0x0c0a, 0x0bc4, 0x0b7e, 0x0b38, 0x0af2, 0x0aad, 0x0a68, 0x0a22, 0x09dd, 0x0998, 0x0953, 0x090f, 0x08ca, 0x0886,
    0x0842, 0x07fd, 0x07b9, 0x0776, 0x0732, 0x06ee, 0x06ab, 0x0668, 0x0624, 0x05e1, 0x059e, 0x055c, 0x0519, 0x04d6, 0x0494, 0x0452,
    0x0410, 0x03ce, 0x038c, 0x034a, 0x0309, 0x02c7, 0x0286, 0x0245, 0x0204, 0x01c3, 0x0182, 0x0141, 0x0101, 0x00c0, 0x0080, 0x0040,
    // clang-format on
};
constexpr auto RSQ_LOOKUP = std::array<uint16_t, 512>{
    // clang-format off
    0xffff, 0xff00, 0xfe02, 0xfd06, 0xfc0b, 0xfb12, 0xfa1a, 0xf923, 0xf82e, 0xf73b, 0xf648, 0xf557, 0xf467, 0xf379, 0xf28c, 0xf1a0,
    0xf0b6, 0xefcd, 0xeee5, 0xedff, 0xed19, 0xec35, 0xeb52, 0xea71, 0xe990, 0xe8b1, 0xe7d3, 0xe6f6, 0xe61b, 0xe540, 0xe467, 0xe38e,
    0xe2b7, 0xe1e1, 0xe10d, 0xe039, 0xdf66, 0xde94, 0xddc4, 0xdcf4, 0xdc26, 0xdb59, 0xda8c, 0xd9c1, 0xd8f7, 0xd82d, 0xd765, 0xd69e,
    0xd5d7, 0xd512, 0xd44e, 0xd38a, 0xd2c8, 0xd206, 0xd146, 0xd086, 0xcfc7, 0xcf0a, 0xce4d, 0xcd91, 0xccd6, 0xcc1b, 0xcb62, 0xcaa9,
    0xc9f2, 0xc93b, 0xc885, 0xc7d0, 0xc71c, 0xc669, 0xc5b6, 0xc504, 0xc453, 0xc3a3, 0xc2f4, 0xc245, 0xc198, 0xc0eb, 0xc03f, 0xbf93,
    0xbee9, 0xbe3f, 0xbd96, 0xbced, 0xbc46, 0xbb9f, 0xbaf8, 0xba53, 0xb9ae, 0xb90a, 0xb867, 0xb7c5, 0xb723, 0xb681, 0xb5e1, 0xb541,
    0xb4a2, 0xb404, 0xb366, 0xb2c9, 0xb22c, 0xb191, 0xb0f5, 0xb05b, 0xafc1, 0xaf28, 0xae8f, 0xadf7, 0xad60, 0xacc9, 0xac33, 0xab9e,
    0xab09, 0xaa75, 0xa9e1, 0xa94e, 0xa8bc, 0xa82a, 0xa799, 0xa708, 0xa678, 0xa5e8, 0xa559, 0xa4cb, 0xa43d, 0xa3b0, 0xa323, 0xa297,
    0xa20b, 0xa180, 0xa0f6, 0xa06c, 0x9fe2, 0x9f59, 0x9ed1, 0x9e49, 0x9dc2, 0x9d3b, 0x9cb4, 0x9c2f, 0x9ba9, 0x9b25, 0x9aa0, 0x9a1c,
    0x9999, 0x9916, 0x9894, 0x9812, 0x9791, 0x9710, 0x968f, 0x960f, 0x9590, 0x9511, 0x9492, 0x9414, 0x9397, 0x931a, 0x929d, 0x9221,
    0x91a5, 0x9129, 0x90af, 0x9034, 0x8fba, 0x8f40, 0x8ec7, 0x8e4f, 0x8dd6, 0x8d5e, 0x8ce7, 0x8c70, 0x8bf9, 0x8b83, 0x8b0d, 0x8a98,
    0x8a23, 0x89ae, 0x893a, 0x88c6, 0x8853, 0x87e0, 0x876d, 0x86fb, 0x8689, 0x8618, 0x85a7, 0x8536, 0x84c6, 0x8456, 0x83e7, 0x8377,
    0x8309, 0x829a, 0x822c, 0x81bf, 0x8151, 0x80e4, 0x8078, 0x800c, 0x7fa0, 0x7f34, 0x7ec9, 0x7e5e, 0x7df4, 0x7d8a, 0x7d20, 0x7cb6,
    0x7c4d, 0x7be5, 0x7b7c, 0x7b14, 0x7aac, 0x7a45, 0x79de, 0x7977, 0x7911, 0x78ab, 0x7845, 0x77df, 0x777a, 0x7715, 0x76b1, 0x764d,
    0x75e9, 0x7585, 0x7522, 0x74bf, 0x745d, 0x73fa, 0x7398, 0x7337, 0x72d5, 0x7274, 0x7213, 0x71b3, 0x7152, 0x70f2, 0x7093, 0x7033,
    0x6fd4, 0x6f76, 0x6f17, 0x6eb9, 0x6e5b, 0x6dfd, 0x6da0, 0x6d43, 0x6ce6, 0x6c8a, 0x6c2d, 0x6bd1, 0x6b76, 0x6b1a, 0x6abf, 0x6a64,
    0x6a09, 0x6955, 0x68a1, 0x67ef, 0x673e, 0x668d, 0x65de, 0x6530, 0x6482, 0x63d6, 0x632b, 0x6280, 0x61d7, 0x612e, 0x6087, 0x5fe0,
    0x5f3a, 0x5e95, 0x5df1, 0x5d4e, 0x5cac, 0x5c0b, 0x5b6b, 0x5acb, 0x5a2c, 0x598f, 0x58f2, 0x5855, 0x57ba, 0x5720, 0x5686, 0x55ed,
    0x5555, 0x54be, 0x5427, 0x5391, 0x52fc, 0x5268, 0x51d5, 0x5142, 0x50b0, 0x501f, 0x4f8e, 0x4efe, 0x4e6f, 0x4de1, 0x4d53, 0x4cc6,
    0x4c3a, 0x4baf, 0x4b24, 0x4a9a, 0x4a10, 0x4987, 0x48ff, 0x4878, 0x47f1, 0x476b, 0x46e5, 0x4660, 0x45dc, 0x4558, 0x44d5, 0x4453,
    0x43d1, 0x434f, 0x42cf, 0x424f, 0x41cf, 0x4151, 0x40d2, 0x4055, 0x3fd8, 0x3f5b, 0x3edf, 0x3e64, 0x3de9, 0x3d6e, 0x3cf5, 0x3c7c,
    0x3c03, 0x3b8b, 0x3b13, 0x3a9c, 0x3a26, 0x39b0, 0x393a, 0x38c5, 0x3851, 0x37dd, 0x3769, 0x36f6, 0x3684, 0x3612, 0x35a0, 0x352f,
    0x34bf, 0x344f, 0x33df, 0x3370, 0x3302, 0x3293, 0x3226, 0x31b9, 0x314c, 0x30df, 0x3074, 0x3008, 0x2f9d, 0x2f33, 0x2ec8, 0x2e5f,
    0x2df6, 0x2d8d, 0x2d24, 0x2cbc, 0x2c55, 0x2bee, 0x2b87, 0x2b21, 0x2abb, 0x2a55, 0x29f0, 0x298b, 0x2927, 0x28c3, 0x2860, 0x27fd,
    0x279a, 0x2738, 0x26d6, 0x2674, 0x2613, 0x25b2, 0x2552, 0x24f2, 0x2492, 0x2432, 0x23d3, 0x2375, 0x2317, 0x22b9, 0x225b, 0x21fe,
    0x21a1, 0x2145, 0x20e8, 0x208d, 0x2031, 0x1fd6, 0x1f7b, 0x1f21, 0x1ec7, 0x1e6d, 0x1e13, 0x1dba, 0x1d61, 0x1d09, 0x1cb1, 0x1c59,
    0x1c01, 0x1baa, 0x1b53, 0x1afc, 0x1aa6, 0x1a50, 0x19fa, 0x19a5, 0x1950, 0x18fb, 0x18a7, 0x1853, 0x17ff, 0x17ab, 0x1758, 0x1705,
    0x16b2, 0x1660, 0x160d, 0x15bc, 0x156a, 0x1519, 0x14c8, 0x1477, 0x1426, 0x13d6, 0x1386, 0x1337, 0x12e7, 0x1298, 0x1249, 0x11fb,
    0x11ac, 0x115e, 0x1111, 0x10c3, 0x1076, 0x1029, 0x0fdc, 0x0f8f, 0x0f43, 0x0ef7, 0x0eab, 0x0e60, 0x0e15, 0x0dca, 0x0d7f, 0x0d34,
    0x0cea, 0x0ca0, 0x0c56, 0x0c0c, 0x0bc3, 0x0b7a, 0x0b31, 0x0ae8, 0x0aa0, 0x0a58, 0x0a10, 0x09c8, 0x0981, 0x0939, 0x08f2, 0x08ab,
    0x0865, 0x081e, 0x07d8, 0x0792, 0x074d, 0x0707, 0x06c2, 0x067d, 0x0638, 0x05f3, 0x05af, 0x056a, 0x0526, 0x04e2, 0x049f, 0x045b,
    0x0418, 0x03d5, 0x0392, 0x0350, 0x030d, 0x02cb, 0x0289, 0x0247, 0x0206, 0x01c4, 0x0183, 0x0142, 0x0101, 0x00c0, 0x0080, 0x0040,
    // clang-format on
};
constexpr auto RSP_DMEM_BASE = Util::rangeOf(Memory::PhysSeg::RSP_DMEM).lower;

export namespace Param {
// clang-format off
enum Accumulator  : uint8_t { ACCUM_NONE, ACCUM_ZERO_EXT, ACCUM_SIGN_EXT };
enum AccumOut     : uint8_t { ACCUM_HI_32, ACCUM_LO_32 };
enum CarryIn      : uint8_t { CARRY_IN_NONE, CARRY_IN };
enum OperandSign  : uint8_t { UNSIGNED, SIGNED };
enum ResultClamp  : uint8_t { CLAMP_NONE, CLAMP_UNSIGNED, CLAMP_SIGNED };
enum ProductAccum : uint8_t { ACCUM_SET, ACCUM_ADD };
enum ProductRound : uint8_t { ROUND_NONE, ROUND };
enum MemoryTypeV  : uint8_t { LOADV, STOREV };
enum Mem128bDir   : uint8_t { QUAD, REST };
enum RecipFunc    : uint8_t { RECIP, RECIP_SQRT };
struct Shift { int8_t value; constexpr operator int8_t() const { return value; } };
// clang-format on
} // namespace Param

export namespace RSP {

class InstructionExecutor {
  public:
    InstructionExecutor(std::shared_ptr<Util::Logger> logger,
                        CPU::Registers<Sys::RSP>*     gprs,
                        RSP::Registers*               vprs,
                        Memory::MemoryBus*            memoryBus)
        : m_logger(logger),
          m_cpuExec(logger, gprs, memoryBus),
          m_gprs(gprs),
          m_vprs(vprs),
          m_memoryBus(memoryBus) {}

    auto cpuExec() {
        return &m_cpuExec;
    }

    template <Param::OperandSign VsVtSign, Param::Accumulator Accum = Param::ACCUM_ZERO_EXT, Param::ResultClamp VdClamp = Param::CLAMP_NONE, typename VcoLoFunc = std::nullptr_t, typename VcoHiFunc = std::nullptr_t, Param::CarryIn Carry = Param::CARRY_IN_NONE, typename Function>
        requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>> &&
                 (std::same_as<std::nullptr_t, VcoLoFunc> || std::integral<std::invoke_result_t<VcoLoFunc, uint32_t>>) &&
                 (std::same_as<std::nullptr_t, VcoHiFunc> || std::integral<std::invoke_result_t<VcoHiFunc, uint32_t>>))
    auto executeBivariate(uint32_t inst, Function&& func, VcoLoFunc vcoLoFunc = nullptr, VcoHiFunc vcoHiFunc = nullptr) -> void;

    template <Param::OperandSign VsVtSign, Param::Accumulator Accum = Param::ACCUM_ZERO_EXT, Param::ResultClamp VdClamp = Param::CLAMP_NONE, typename Function>
        requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>)
    auto executeBivariateWithCarryIn(uint32_t inst, Function&& func) -> void {
        executeBivariate<VsVtSign, Accum, VdClamp, std::nullptr_t, std::nullptr_t, Param::CARRY_IN, Function>(inst, std::forward<Function>(func));
    }

    template <Param::ResultClamp VdClamp, Param::OperandSign VsSign, Param::OperandSign VtSign, Param::ProductAccum Accum, Param::AccumOut AccumOut, Param::Shift ShiftValue = Param::Shift(0), Param::ProductRound Round = Param::ROUND_NONE>
        requires(VdClamp != Param::ResultClamp::CLAMP_NONE)
    auto executeMultiply(uint32_t inst) -> void;

    template <Param::MemoryTypeV Type, std::size_t Log2Size>
        requires(Log2Size == 0uz || Log2Size == 1uz || Log2Size == 2uz || Log2Size == 3uz)
    auto executeLoadStore(uint32_t inst) -> void;

    template <Param::MemoryTypeV Type, Param::OperandSign Sign>
    auto executeLoadStorePacked(uint32_t inst) -> void;

    template <Param::MemoryTypeV Type, Param::Mem128bDir Dir>
    auto executeLoadStoreQuad(uint32_t inst) -> void;

    template <Param::Accumulator Accum, typename Function>
        requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>)
    auto executeSingleLane(uint32_t inst, Function&& func) -> void;

    auto executeSingleLaneMove(uint32_t inst) -> void;

    template <typename Function> // vcc <- f(vs, vt, vco low, vco high)
        requires(std::integral<std::invoke_result_t<Function, int16_t, int16_t, bool, bool>>)
    auto executeSelectCompare(uint32_t inst, Function&& func) -> void;

    auto executeSelectMerge(uint32_t inst) -> void;

    auto executeReadAccumulators(uint32_t inst) -> void;

    auto executeReciprocalHigh(uint32_t inst) -> void;

    template <Param::RecipFunc Func>
    auto executeReciprocalLow(uint32_t inst) -> void;

    auto executeSelectClipHigh(uint32_t inst) -> void;

    auto executeSelectClipLow(uint32_t inst) -> void;

    auto executeSelectCrimp(uint32_t inst) -> void;

  private:
    template <std::integral T>
    auto readDMem(uint32_t addr) const -> T;

    template <std::integral T>
    auto writeDMem(uint32_t addr, T value) -> void;

    std::shared_ptr<Util::Logger>      m_logger;
    CPU::InstructionExecutor<Sys::RSP> m_cpuExec;
    CPU::Registers<Sys::RSP>*          m_gprs{};
    RSP::Registers*                    m_vprs{};
    Memory::MemoryBus*                 m_memoryBus{};
};

template <Param::OperandSign VsVtSign, Param::Accumulator Accum, Param::ResultClamp VdClamp, typename VcoLoFunc, typename VcoHiFunc, Param::CarryIn Carry, typename Function>
    requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>> &&
             (std::same_as<std::nullptr_t, VcoLoFunc> || std::integral<std::invoke_result_t<VcoLoFunc, uint32_t>>) &&
             (std::same_as<std::nullptr_t, VcoHiFunc> || std::integral<std::invoke_result_t<VcoHiFunc, uint32_t>>))
auto InstructionExecutor::executeBivariate(uint32_t inst, Function&& func, VcoLoFunc vcoLoFunc, VcoHiFunc vcoHiFunc) -> void {
    using VsVtRegType = std::conditional_t<VsVtSign == Param::SIGNED, int16_t, uint16_t>;
    const auto op     = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vsVpr  = m_vprs->readVpr<VsVtRegType>(op.vs);
    const auto vtVpr  = m_vprs->readVpr<VsVtRegType>(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));

    using VprType = std::conditional_t<Accum == Param::Accumulator::ACCUM_SIGN_EXT, int16_t, uint16_t>;

    constexpr auto hasVcoLoFunc = !std::same_as<std::nullptr_t, VcoLoFunc>;
    constexpr auto hasVcoHiFunc = !std::same_as<std::nullptr_t, VcoHiFunc>;

    static_assert(Carry != Param::CarryIn::CARRY_IN || (!hasVcoLoFunc && !hasVcoHiFunc), "Cannot use carry-in with vco functions");

    auto vco = std::bitset<16>{};
    if constexpr (Carry == Param::CarryIn::CARRY_IN) {
        vco = m_vprs->readVco();
    }
    auto accums = std::array<RSP::Accumulator, 8>{};
    if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
        accums = m_vprs->readAccumulators();
    }

    auto result = VPR<VprType>{};
    for (auto i = 0uz; i < 8; ++i) {
        auto value = func(vsVpr[i], vtVpr[i]);
        if constexpr (Carry == Param::CarryIn::CARRY_IN) {
            value = func(value, vco[i]);
        }
        if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
            accums[i].low = static_cast<uint16_t>(value);
        }
        if constexpr (VdClamp == Param::ResultClamp::CLAMP_SIGNED) {
            value = Util::clamp<int16_t>(value);
        } else if constexpr (VdClamp == Param::ResultClamp::CLAMP_UNSIGNED) {
            value = Util::clamp<uint16_t>(value);
        }
        result[i] = value;
        if constexpr (hasVcoLoFunc) {
            vco[i] = static_cast<bool>(vcoLoFunc(value) & 1);
        }
        if constexpr (hasVcoHiFunc) {
            vco[i + 8] = static_cast<bool>(vcoHiFunc(value) & 1);
        }
    }

    if constexpr (hasVcoLoFunc || hasVcoHiFunc) {
        m_vprs->writeVco(vco);
    } else if constexpr (Carry == Param::CarryIn::CARRY_IN) {
        m_vprs->clearVco();
    }
    if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
        m_vprs->writeAccumulators(accums);
    }
    m_vprs->writeVpr(op.vd, result);
}

template <Param::ResultClamp VdClamp, Param::OperandSign VsSign, Param::OperandSign VtSign, Param::ProductAccum Accum, Param::AccumOut AccumOut, Param::Shift ShiftValue, Param::ProductRound Round>
    requires(VdClamp != Param::ResultClamp::CLAMP_NONE)
auto InstructionExecutor::executeMultiply(uint32_t inst) -> void {
    using VsRegType   = std::conditional_t<VsSign == Param::SIGNED, int16_t, uint16_t>;
    using VtRegType   = std::conditional_t<VtSign == Param::SIGNED, int16_t, uint16_t>;
    using VdClampType = std::conditional_t<VdClamp == Param::CLAMP_SIGNED, int16_t, uint16_t>;

    const auto op    = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vsVpr = m_vprs->readVpr<VsRegType>(op.vs);
    const auto vtVpr = m_vprs->readVpr<VtRegType>(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));

    auto accums = std::array<RSP::Accumulator, 8>{};
    if constexpr (Accum != Param::ProductAccum::ACCUM_SET) {
        accums = m_vprs->readAccumulators();
    }

    auto result = VPR<uint16_t>{};
    for (auto i = 0uz; i < 8; ++i) {
        // to preserve sign, the signed type must be larger than the unsigned
        using VsExtType = std::conditional_t<VsSign == Param::SIGNED, int64_t, uint32_t>;
        using VtExtType = std::conditional_t<VtSign == Param::SIGNED, int64_t, uint32_t>;
        auto product    = static_cast<int64_t>(static_cast<VsExtType>(vsVpr[i]) * static_cast<VtExtType>(vtVpr[i]));

        if constexpr (ShiftValue < 0) {
            product >>= -ShiftValue;
        } else if constexpr (ShiftValue > 0) {
            product <<= ShiftValue;
        }
        if constexpr (Round == Param::ROUND) {
            product += 0x8000;
        }
        if constexpr (Accum == Param::ProductAccum::ACCUM_SET) {
            accums[i] = static_cast<int64_t>(product);
        } else { // ACCUM_ADD
            accums[i] = static_cast<int64_t>(accums[i]) + product;
        }

        const auto accumVal = static_cast<int64_t>(accums[i]);
        auto       out      = accumVal;
        if (accumVal > std::numeric_limits<int32_t>::max()) {
            out = std::numeric_limits<VdClampType>::max();
        } else if (accumVal < std::numeric_limits<int32_t>::min()) {
            out = std::numeric_limits<VdClampType>::min();
        } else {
            if constexpr (AccumOut == Param::ACCUM_HI_32) {
                out >>= 16;
            }
            out &= 0xFFFF;
        }
        result[i] = out;
    }

    m_vprs->writeAccumulators(accums);
    m_vprs->writeVpr(op.vd, result);
}

template <Param::MemoryTypeV Type, std::size_t Log2Size>
    requires(Log2Size == 0uz || Log2Size == 1uz || Log2Size == 2uz || Log2Size == 3uz)
auto InstructionExecutor::executeLoadStore(uint32_t inst) -> void {
    const auto ops        = std::bit_cast<ISA::RSP::TypeVI>(inst);
    const auto signExtImm = static_cast<int32_t>(static_cast<int8_t>(ops.imm << 1) >> 1);
    const auto addr       = static_cast<uint32_t>((signExtImm << Log2Size) + m_gprs->readGpr(ops.rs));
    const auto byteRange  = std::views::iota(ops.vtElem, std::min(16uz, ops.vtElem + (1uz << Log2Size)));

    auto vt = m_vprs->readVpr(ops.vt);
    if constexpr (Type == Param::LOADV) {
        const auto numBytes = std::min(16uz - ops.vtElem, 1uz << Log2Size);
        for (const auto i : std::views::iota(0uz, numBytes)) {
            const auto data    = readDMem<uint8_t>(addr + i);
            const auto byteIdx = ops.vtElem + i;
            vt.setByte(byteIdx, static_cast<uint8_t>(data));
        }
    } else if constexpr (Type == Param::STOREV) {
        constexpr auto numBytes = 1uz << Log2Size;
        for (const auto i : std::views::iota(0uz, numBytes)) {
            const auto byteIdx = (ops.vtElem + i) % 16;
            const auto data    = vt.getByte(byteIdx);
            writeDMem<uint8_t>(addr + i, data);
        }
    }
    if constexpr (Type == Param::LOADV) {
        m_vprs->writeVpr(ops.vt, vt);
    }
}

template <Param::MemoryTypeV Type, Param::OperandSign Sign>
auto InstructionExecutor::executeLoadStorePacked(uint32_t inst) -> void {
    const auto ops        = std::bit_cast<ISA::RSP::TypeVI>(inst);
    const auto signExtImm = static_cast<int32_t>(static_cast<int8_t>(ops.imm << 1) >> 1);
    const auto addr       = static_cast<uint32_t>((signExtImm << 3) + m_gprs->readGpr(ops.rs));

    auto vt = ::RSP::VPR<uint16_t>();
    if constexpr (Type == Param::STOREV) {
        vt = m_vprs->readVpr(ops.vt);
    }
    for (const auto i : std::views::iota(0, 8)) {
        const auto vtIdx    = ops.vtElem + i;
        const auto addrWrap = (addr & ~0b111) + ((addr + i) % 8); // wrap around the current 8-byte block
        auto&      vtElem   = vt[vtIdx % 8];
        if constexpr (Type == Param::LOADV) {
            const auto byte = readDMem<uint8_t>(addrWrap);
            if constexpr (Sign == Param::SIGNED) {
                vtElem = byte << 8;
            } else {
                vtElem = byte << 7;
            }
        } else {
            auto byte = uint8_t();
            if ((Sign == Param::SIGNED && vtIdx < 8) || (Sign == Param::UNSIGNED && vtIdx >= 8)) {
                byte = static_cast<uint8_t>(vtElem >> 8);
            } else {
                byte = static_cast<uint8_t>(vtElem >> 7);
            }
            writeDMem<uint8_t>(addrWrap, byte);
        }
    }
    if constexpr (Type == Param::LOADV) {
        m_vprs->writeVpr(ops.vt, vt);
    }
}

template <Param::MemoryTypeV Type, Param::Mem128bDir Dir>
auto InstructionExecutor::executeLoadStoreQuad(uint32_t inst) -> void {
    const auto ops        = std::bit_cast<ISA::RSP::TypeVI>(inst);
    const auto signExtImm = static_cast<int32_t>(static_cast<int8_t>(ops.imm << 1) >> 1);
    const auto addr       = static_cast<uint32_t>((signExtImm << 4) + m_gprs->readGpr(ops.rs));
    auto       vt         = m_vprs->readVpr(ops.vt);

    const auto addrBytes = [addr] {
        const auto base = static_cast<std::size_t>(addr);
        if constexpr (Dir == Param::QUAD) {
            // addr, addr + 1 ... next aligned address
            return std::views::iota(base, base + 16 - (base % 16));
        } else {
            // addr, addr - 1 ... previous aligned address
            return std::views::iota(base - (base % 16), base + 1) | std::views::reverse;
        }
    }();

    for (auto [i, addrByte] : std::views::enumerate(addrBytes)) {
        const auto vprByte = i + ops.vtElem * 2;
        if constexpr (Type == Param::LOADV) {
            if (vprByte >= 16) {
                break;
            }
            const auto data = readDMem<uint8_t>(addrByte);
            vt.setByte(vprByte, data);
        } else {
            const auto data = vt.getByte(vprByte % 16);
            writeDMem<uint8_t>(addrByte, data);
        }
    }
    if constexpr (Type == Param::LOADV) {
        m_vprs->writeVpr(ops.vt, vt);
    }
}

template <Param::Accumulator Accum, typename Function>
    requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>)
auto InstructionExecutor::executeSingleLane(uint32_t inst, Function&& func) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVS>(inst);

    const auto vtLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vtElem));
    const auto vdLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vdElem));

    const auto vt = m_vprs->readVpr<uint16_t>(ops.vt);
    auto       vd = m_vprs->readVpr<uint16_t>(ops.vd);

    const auto result = func(vt[vtLane], 0);
    vd[vdLane]        = result;

    if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
        auto accs = m_vprs->readAccumulators();
        for (auto& acc : accs) {
            acc.low = static_cast<uint16_t>(result);
        }
        m_vprs->writeAccumulators(accs);
    }
    m_vprs->writeVpr(ops.vd, vd);
}

auto InstructionExecutor::executeSingleLaneMove(uint32_t inst) -> void {
    const auto ops    = std::bit_cast<ISA::RSP::TypeVS>(inst);
    const auto vdLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vdElem));
    const auto vtElem = static_cast<ISA::VEC_ELEM>(ops.vtElem);
    const auto vt     = m_vprs->readVpr<uint16_t>(ops.vt, vtElem);

    // write vd
    auto vd    = m_vprs->readVpr<uint16_t>(ops.vd);
    vd[vdLane] = vt[vdLane]; // note: vt intentionally uses the vd lane
    m_vprs->writeVpr(ops.vd, vd);

    // load accumulators
    auto accs = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        accs[i].low = vt[i];
    }
    m_vprs->writeAccumulators(accs);
}

template <typename Function>
    requires(std::integral<std::invoke_result_t<Function, int16_t, int16_t, bool, bool>>)
auto InstructionExecutor::executeSelectCompare(uint32_t inst, Function&& func) -> void {
    const auto op  = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vs  = m_vprs->readVpr<int16_t>(op.vs);
    const auto vt  = m_vprs->readVpr<int16_t>(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));
    const auto vco = m_vprs->readVco();

    auto vd   = ::RSP::VPR<int16_t>{};
    auto vcc  = std::bitset<16>();
    auto accs = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        vcc[i] = func(vs[i], vt[i], vco[i], vco[i + 8]);

        const auto result = static_cast<int16_t>(vcc[i] ? vs[i] : vt[i]);
        accs[i].low       = static_cast<uint16_t>(result);
        vd[i]             = result;
    }
    m_vprs->writeVcc(vcc);
    m_vprs->writeAccumulators(accs);
    m_vprs->writeVpr(op.vd, vd);
    m_vprs->clearVco();
    m_vprs->clearVce();
}

auto InstructionExecutor::executeSelectMerge(uint32_t inst) -> void {
    const auto op  = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vcc = m_vprs->readVcc();
    const auto vs  = m_vprs->readVpr<uint16_t>(op.vs);
    const auto vt  = m_vprs->readVpr<uint16_t>(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));

    auto vd   = ::RSP::VPR<uint16_t>();
    auto accs = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        vd[i]       = vcc[i] ? vs[i] : vt[i];
        accs[i].low = vd[i];
    }
    m_vprs->clearVco();
    m_vprs->writeVpr(op.vd, vd);
    m_vprs->writeAccumulators(accs);
}

auto InstructionExecutor::executeReadAccumulators(uint32_t inst) -> void {
    const auto ops    = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto accums = m_vprs->readAccumulators();

    auto vd = ::RSP::VPR<uint16_t>();
    for (auto i : std::views::iota(0, 8)) {
        switch (static_cast<ISA::VEC_ELEM>(ops.vtElem)) {
            case ISA::VEC_ELEM::e0:
                vd[i] = accums[i].low;
                break;
            case ISA::VEC_ELEM::e1:
                vd[i] = accums[i].mid;
                break;
            case ISA::VEC_ELEM::e2:
                vd[i] = accums[i].high;
                break;
            default: throw Util::Error("Invalid element {} for VSAR instruction", static_cast<int>(ops.vtElem));
        }
    };
    m_vprs->writeVpr(ops.vd, vd);
}

auto InstructionExecutor::executeReciprocalHigh(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVS>(inst);

    // read VT into DivIn
    const auto vtLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vtElem));
    const auto vt     = m_vprs->readVpr<uint16_t>(ops.vt);
    m_vprs->writeDivIn(static_cast<uint32_t>(vt[vtLane]) << 16);

    // read VT into Accumulators
    auto accums = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        accums[i].low = vt[i];
    }
    m_vprs->writeAccumulators(accums);

    // write VD with DivOut
    const auto divOut = static_cast<uint16_t>(m_vprs->readDivOut() >> 16);
    const auto vdLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vdElem));
    auto       vd     = m_vprs->readVpr<uint16_t>(ops.vd);
    vd[vdLane]        = divOut;
    m_vprs->writeVpr(ops.vd, vd);
}

template <Param::RecipFunc Func>
auto InstructionExecutor::executeReciprocalLow(uint32_t inst) -> void {
    const auto ops   = std::bit_cast<ISA::RSP::TypeVS>(inst);
    const auto vt    = m_vprs->readVpr<uint16_t>(ops.vt)[ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vtElem))];
    const auto input = static_cast<int32_t>((m_vprs->readDivIn().value_or(vt >> 15 ? 0xFFFF : 0) << 16) |
                                            static_cast<uint32_t>(vt));

    auto result = uint32_t();
    if (input == 0) {
        result = std::numeric_limits<int32_t>::max();
    } else if (input == std::numeric_limits<int16_t>::min()) {
        result = 0xFFFF0000;
    } else {
        const auto absInput = static_cast<uint32_t>(std::abs(static_cast<int64_t>(input)));
        const auto shift    = std::countl_zero(absInput);
        if constexpr (Func == Param::RecipFunc::RECIP) {
            const auto index = (static_cast<uint64_t>(absInput) << (shift + 1)) >> 23; // get 9 bits below first set bit
            result           = ((1u << 16 | RCP_LOOKUP[index]) << 14) >> (31 - shift);
        } else {
            const auto index = (static_cast<uint64_t>(absInput) << shift & 0x7FC00000) >> 22;
            result           = ((1u << 16 | RSQ_LOOKUP[(index & 0x1FE) | (shift & 1)]) << 14) >> ((31 - shift) >> 1);
        }
        if (input < 0) {
            result = ~result;
        }
    }

    // read VT into Accumulators
    auto accums = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        accums[i].low = vt;
    }
    m_vprs->writeAccumulators(accums);

    auto       vdVpr  = m_vprs->readVpr<uint16_t>(ops.vd);
    const auto vdElem = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vdElem));
    vdVpr[vdElem]     = static_cast<uint16_t>(result);
    m_vprs->writeVpr(ops.vd, vdVpr);
    m_vprs->writeDivOut(result);
    m_vprs->writeDivIn(std::nullopt);
}

auto InstructionExecutor::executeSelectClipHigh(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vs  = m_vprs->readVpr<int16_t>(ops.vs);
    const auto vt  = m_vprs->readVpr<int16_t>(ops.vt, static_cast<ISA::VEC_ELEM>(ops.vtElem));

    auto vd     = ::RSP::VPR<int16_t>();
    auto accums = m_vprs->readAccumulators();

    m_vprs->clearVcc();
    m_vprs->clearVco();
    m_vprs->clearVce();

    auto vcc = std::bitset<16>{};
    auto vco = std::bitset<16>{};
    auto vce = std::bitset<8>{};

    for (auto i : std::views::iota(0, 8)) {
        const auto sign = (vs[i] & 0x8000) != (vt[i] & 0x8000);

        auto sum = int16_t();
        if (sign) {
            sum           = vs[i] + vt[i];
            accums[i].low = sum <= 0 ? -vt[i] : vs[i];
            vcc[i]        = sum <= 0;
            vcc[i + 8]    = vt[i] < 0;
        } else {
            sum           = vs[i] - vt[i];
            accums[i].low = sum >= 0 ? vt[i] : vs[i];
            vcc[i]        = vt[i] < 0;
            vcc[i + 8]    = sum >= 0;
        }
        vco[i]     = sign;
        vco[i + 8] = sum != 0 && vs[i] != (vt[i] ^ 0xffff);
        vce[i]     = sign ? (sum == -1) : 0;
        vd[i]      = accums[i].low;
    }
    m_vprs->writeAccumulators(accums);
    m_vprs->writeVpr(ops.vd, vd);
    m_vprs->writeVcc(vcc);
    m_vprs->writeVco(vco);
    m_vprs->writeVce(vce);
}

auto InstructionExecutor::executeSelectClipLow(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vs  = m_vprs->readVpr<uint16_t>(ops.vs);
    const auto vt  = m_vprs->readVpr<uint16_t>(ops.vt, static_cast<ISA::VEC_ELEM>(ops.vtElem));
    auto       vcc = m_vprs->readVcc();
    const auto vco = m_vprs->readVco();
    const auto vce = m_vprs->readVce();

    auto vd     = ::RSP::VPR<uint16_t>();
    auto accums = m_vprs->readAccumulators();

    for (auto i : std::views::iota(0, 8)) {
        const auto sign = vco[i];
        auto       acc  = uint16_t();
        if (sign) {
            if (vco[i + 8]) {
                acc = vcc[i] ? -vt[i] : vs[i];
            } else {
                const auto sum   = static_cast<uint32_t>(vs[i]) + static_cast<uint32_t>(vt[i]);
                const auto carry = sum > std::numeric_limits<uint16_t>::max();
                if (vce[i]) {
                    vcc[i] = !sum || !carry;
                } else {
                    vcc[i] = !sum && !carry;
                }
                acc = vcc[i] ? -vt[i] : vs[i];
            }
        } else {
            if (vco[i + 8]) {
                acc = vcc[i + 8] ? vt[i] : vs[i];
            } else {
                vcc[i + 8] = vs[i] >= vt[i];
                acc        = vcc[i + 8] ? vt[i] : vs[i];
            }
        }
        accums[i].low = acc;
        vd[i]         = accums[i].low;
    }
    m_vprs->writeAccumulators(accums);
    m_vprs->writeVpr(ops.vd, vd);
    m_vprs->writeVcc(vcc);
    m_vprs->clearVco();
    m_vprs->clearVce();
}

auto InstructionExecutor::executeSelectCrimp(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vs  = m_vprs->readVpr<int16_t>(ops.vs);
    const auto vt  = m_vprs->readVpr<int16_t>(ops.vt, static_cast<ISA::VEC_ELEM>(ops.vtElem));

    auto vd     = ::RSP::VPR<int16_t>();
    auto accums = m_vprs->readAccumulators();

    m_vprs->clearVcc();

    auto vcc = std::bitset<16>{};
    auto vco = std::bitset<16>{};
    auto vce = std::bitset<8>{};

    for (auto i : std::views::iota(0, 8)) {
        const auto sign = (vs[i] ^ vt[i]) < 0;

        auto sum = int16_t();
        if (sign) {
            vcc[i]        = (vs[i] + vt[i] + 1) <= 0;
            vcc[i + 8]    = vt[i] < 0;
            accums[i].low = vcc[i] ? ~vt[i] : vs[i];
        } else {
            vcc[i]        = vt[i] < 0;
            vcc[i + 8]    = vs[i] - vt[i] >= 0;
            accums[i].low = vcc[i + 8] ? vt[i] : vs[i];
        }
        vco[i]     = sign;
        vco[i + 8] = sum != 0;
        vce[i]     = sign ? (sum == -1) : 0;
        vd[i]      = accums[i].low;
    }
    m_vprs->writeAccumulators(accums);
    m_vprs->writeVpr(ops.vd, vd);
    m_vprs->clearVco();
    m_vprs->clearVce();
}

template <std::integral T>
auto InstructionExecutor::readDMem(uint32_t addr) const -> T {
    addr = (addr & 0xFFF) + RSP_DMEM_BASE;

    const auto value = m_memoryBus->readPhysical<T>(addr);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::RSP>(
            std::tuple{"op", "r"},
            std::tuple{"addr", HEXFMT12, addr - RSP_DMEM_BASE},
            Memory::makePrintData(value));
    }
    return value;
}

template <std::integral T>
auto InstructionExecutor::writeDMem(uint32_t addr, T value) -> void {
    addr = (addr & 0xFFF) + RSP_DMEM_BASE;
    m_memoryBus->writePhysical<T>(addr, value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::RSP>(
            std::tuple{"op", "w"},
            std::tuple{"addr", HEXFMT12, addr - RSP_DMEM_BASE},
            Memory::makePrintData(value));
    }
}

} // namespace RSP