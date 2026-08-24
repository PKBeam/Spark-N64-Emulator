module;

#include <util/defines.hpp>

export module RSP:Registers;

import std;
import ISA;
import Util;

export namespace RSP {

struct Registers {
    using VPR = std::array<uint16_t, 8>;
    struct Accumulator {
        uint64_t low  : 16;
        uint64_t mid  : 16;
        uint64_t high : 16;
        uint64_t      : 16;
    };

    Registers(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {}

    auto readVpr(std::size_t index, ISA::VEC_ELEM elem = ISA::VEC_ELEM::NONE_0) -> VPR;

    auto writeVpr(std::size_t index, VPR value, ISA::VEC_ELEM elem = ISA::VEC_ELEM::NONE_0) -> void;

    auto apply(std::size_t vd, std::size_t vs, std::size_t vt, ISA::VEC_ELEM vtElem, std::function<uint16_t(uint16_t, uint16_t)> func) -> void;

  private:
    template <std::meta::info vecElem>
    consteval auto getLanesForElement() {
        auto result = std::array<int, 8>{};
        template for (auto i = 0; constexpr auto a : Util::staticAnnotationsOf(vecElem)) {
            result[i++] = std::meta::extract<int>(a);
        }
        return result;
    }

    std::shared_ptr<Util::Logger> m_logger;

    std::array<VPR, 32>        m_vprs{};
    std::array<Accumulator, 8> m_accums{};
    uint16_t                   m_vcc{};
    uint16_t                   m_vco{};
    uint16_t                   m_vce{};
};

auto Registers::readVpr(std::size_t index, ISA::VEC_ELEM elem) -> VPR {
    VPR result{};
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^ISA::VEC_ELEM)) {
        if (elem == std::meta::extract<ISA::VEC_ELEM>(e)) {
            constexpr auto lanes = getLanesForElement<e>();
            template for (constexpr auto i : std::views::iota(0uz, lanes.size())) {
                result[i] = m_vprs[index][lanes[i]];
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::MED, Sys::RSP>(
                    std::tuple{"op", "read"},
                    std::tuple{"reg", "v{}", index},
                    std::tuple{"data", "{:n:#06X}", result},
                    std::tuple{"lanes", "[{:n}]", lanes});
            }
            return result;
        }
    }
    throw Util::Error("Invalid element value {}", static_cast<int>(elem));
}

auto Registers::writeVpr(std::size_t index, VPR value, ISA::VEC_ELEM elem) -> void {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^ISA::VEC_ELEM)) {
        if (elem == std::meta::extract<ISA::VEC_ELEM>(e)) {
            constexpr auto lanes = getLanesForElement<e>();
            template for (constexpr auto i : std::views::iota(0uz, lanes.size())) {
                m_vprs[index][lanes[i]] = value[i];
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::MED, Sys::RSP>(
                    std::tuple{"op", "write"},
                    std::tuple{"reg", "v{}", index},
                    std::tuple{"data", "0x{:n:04X}", value},
                    std::tuple{"lanes", "[{:n}]", lanes});
            }
        }
    }
}

} // namespace RSP