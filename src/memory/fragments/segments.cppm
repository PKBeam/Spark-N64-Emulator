export module Memory:Segments;

import std;
import Util;

export namespace Memory {

// clang-format off
enum class VirtSeg {
    // order most likely accesses first
    KSEG1 [[=Util::Range{0xA0000000, 0xBFFFFFFF}]],
    KSEG0 [[=Util::Range{0x80000000, 0x9FFFFFFF}]],

    KUSEG [[=Util::Range{0x00000000, 0x7FFFFFFF}]],
    KSSEG [[=Util::Range{0xC0000000, 0xDFFFFFFF}]],
    KSEG3 [[=Util::Range{0xE0000000, 0xFFFFFFFF}]],
};

enum class PhysSeg {
    // RDRAM                [[=Util::Range{0x00000000, 0x003FFFFF}]],
    // RDRAM_EXPANSION_PAK  [[=Util::Range{0x00400000, 0x007FFFFF}]],
    RDRAM                [[=Util::Range{0x00000000, 0x007FFFFF}]],
    RDRAM_REG            [[=Util::Range{0x03F00000, 0x03FFFFFF}]],
    RSP_DMEM             [[=Util::Range{0x04000000, 0x04000FFF}]],
    RSP_IMEM             [[=Util::Range{0x04001000, 0x04001FFF}]],
    RSP_REG              [[=Util::Range{0x04040000, 0x040FFFFF}]],
    RDP_CMD_REG          [[=Util::Range{0x04100000, 0x041FFFFF}]],
    RSP_SPAN_REG         [[=Util::Range{0x04200000, 0x042FFFFF}]],
    MIPS_INTERFACE       [[=Util::Range{0x04300000, 0x043FFFFF}]],
    VIDEO_INTERFACE      [[=Util::Range{0x04400000, 0x044FFFFF}]],
    AUDIO_INTERFACE      [[=Util::Range{0x04500000, 0x045FFFFF}]],
    PERIPHERAL_INTERFACE [[=Util::Range{0x04600000, 0x046FFFFF}]],
    RDRAM_INTERFACE      [[=Util::Range{0x04700000, 0x047FFFFF}]],
    SERIAL_INTERFACE     [[=Util::Range{0x04800000, 0x048FFFFF}]],
    PI_BUS               [[=Util::Range{0x05000000, 0x1FBFFFFF}]],
    SI_BUS               [[=Util::Range{0x1FC00000, 0x1FC007FF}]],
};
// clang-format on

template <typename T>
    requires std::is_enum_v<T>
constexpr auto rangeOf(T seg) -> Util::Range {
    constexpr static auto enumerators = Util::staticEnumeratorsOf(^^T);
    template for (constexpr auto e : enumerators) {
        if (seg == std::meta::extract<T>(e)) {
            return std::meta::extract<Util::Range>(Util::annotationOf(e));
        }
    }
    throw Util::Error("No range found for segment {}", Util::enumName(seg).value_or("unknown"));
}

} // namespace Memory

export using VirtualAddr  = uint32_t;
export using PhysicalAddr = uint32_t;