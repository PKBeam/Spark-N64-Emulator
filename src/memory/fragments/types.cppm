export module Memory:Types;

import std;
import Util;

export namespace Memory {

struct Range {
    uint32_t lower;
    uint32_t upper;

    constexpr auto size() const -> std::size_t {
        return upper - lower + 1;
    }
    constexpr auto contains(uint32_t addr) const -> bool {
        return lower <= addr && addr <= upper;
    }
};

// clang-format off
enum class VirtSeg {
    KUSEG [[=Range{0x00000000, 0x7FFFFFFF}]],
    KSEG0 [[=Range{0x80000000, 0x9FFFFFFF}]],
    KSEG1 [[=Range{0xA0000000, 0xBFFFFFFF}]],
    KSSEG [[=Range{0xC0000000, 0xDFFFFFFF}]],
    KSEG3 [[=Range{0xE0000000, 0xFFFFFFFF}]],
};

enum class PhysSeg {
    RDRAM                [[=Range{0x00000000, 0x003FFFFF}]],
    RDRAM_EXPANSION_PAK  [[=Range{0x00400000, 0x007FFFFF}]],
    RDRAM_REG            [[=Range{0x03F00000, 0x03FFFFFF}]],
    RSP_DMEM             [[=Range{0x04000000, 0x04000FFF}]],
    RSP_IMEM             [[=Range{0x04001000, 0x04001FFF}]],
    RSP_REG              [[=Range{0x04040000, 0x040FFFFF}]],
    RDP_CMD_REG          [[=Range{0x04100000, 0x041FFFFF}]],
    RSP_SPAN_REG         [[=Range{0x04200000, 0x042FFFFF}]],
    MIPS_INTERFACE       [[=Range{0x04300000, 0x043FFFFF}]],
    VIDEO_INTERFACE      [[=Range{0x04400000, 0x044FFFFF}]],
    AUDIO_INTERFACE      [[=Range{0x04500000, 0x045FFFFF}]],
    PERIPHERAL_INTERFACE [[=Range{0x04600000, 0x046FFFFF}]],
    RDRAM_INTERFACE      [[=Range{0x04700000, 0x047FFFFF}]],
    SERIAL_INTERFACE     [[=Range{0x04800000, 0x048FFFFF}]],
    N64DD_CTRL_REG       [[=Range{0x05000000, 0x05FFFFFF}]],
    N64DD_IPL_ROM        [[=Range{0x06000000, 0x07FFFFFF}]],
    SRAM                 [[=Range{0x08000000, 0x0FFFFFFF}]],
    ROM                  [[=Range{0x10000000, 0x1FBFFFFF}]],
    PIF_ROM              [[=Range{0x1FC00000, 0x1FC007BF}]],
    PIF_RAM              [[=Range{0x1FC007C0, 0x1FC007FF}]],
};
// clang-format on

constexpr auto getRange(PhysSeg seg) -> Range {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^PhysSeg)) {
        if ([:e:] == seg) {
            constexpr auto ann = std::meta::annotations_of(e).front();
            return std::meta::extract<Range>(ann);
        }
    }
    throw std::runtime_error("Unknown physical memory segment");
}
} // namespace Memory

export using VirtualAddr  = uint32_t;
export using PhysicalAddr = uint32_t;