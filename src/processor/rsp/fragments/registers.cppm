module;

#include <util/defines.hpp>

export module RSP:Registers;

import std;
import ISA;
import Util;

export namespace RSP {

struct Registers {
    template <std::integral T>
        requires(sizeof(T) == 2)
    using VPR = std::array<T, 8>;

    struct Accumulator {
        uint64_t low  : 16;
        uint64_t mid  : 16;
        uint64_t high : 16;
        uint64_t      : 16;
    };

    struct Control {
        uint16_t low  : 8;
        uint16_t high : 8;
    };

    Registers(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {}

    auto readVpr(std::size_t index, ISA::VEC_ELEM elem = ISA::VEC_ELEM::NONE_0) -> VPR<uint16_t>;
    auto writeVpr(std::size_t index, VPR<uint16_t> value, ISA::VEC_ELEM elem = ISA::VEC_ELEM::NONE_0) -> void;

    auto readVcc() -> Control;
    auto writeVcc(Control value) -> void;

    auto readVco() -> Control;
    auto writeVco(Control value) -> void;

    auto readVce() -> Control;
    auto writeVce(Control value) -> void;

    auto readAccumulator(std::size_t index) -> uint64_t;

    template <std::integral T>
    auto writeAccumulator(std::size_t index, T value) -> void;

    auto readAccumulators() -> std::array<Accumulator, 8>;
    template <std::integral T>
    auto writeAccumulators(VPR<T> vpr) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;

    std::array<VPR<uint16_t>, 32> m_vprs{};
    std::array<Accumulator, 8>    m_accums{};
    Control                       m_vcc{};
    Control                       m_vco{};
    Control                       m_vce{};

    template <std::meta::info vecElem>
    consteval auto getLanesForElement() {
        auto result = std::array<int, 8>{};
        template for (auto i = 0; constexpr auto a : Util::staticAnnotationsOf(vecElem)) {
            result[i++] = std::meta::extract<int>(a);
        }
        return result;
    }
};

auto Registers::readVpr(std::size_t index, ISA::VEC_ELEM elem) -> VPR<uint16_t> {
    VPR<uint16_t> result{};
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
                    std::tuple{"data", "{:n:#06x}", result},
                    std::tuple{"lanes", "[{:n}]", lanes});
            }
            return result;
        }
    }
    throw Util::Error("Invalid element value {}", static_cast<int>(elem));
}

auto Registers::writeVpr(std::size_t index, VPR<uint16_t> value, ISA::VEC_ELEM elem) -> void {
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
                    std::tuple{"data", "{:n:#06x}", value},
                    std::tuple{"lanes", "[{:n}]", lanes});
            }
        }
    }
}

auto Registers::readVcc() -> Registers::Control {
    return m_vcc;
}

auto Registers::writeVcc(Registers::Control value) -> void {
    m_vcc = value;
}

auto Registers::readVco() -> Registers::Control {
    return m_vco;
}

auto Registers::writeVco(Registers::Control value) -> void {
    m_vco = value;
}

auto Registers::readVce() -> Registers::Control {
    return m_vce;
}

auto Registers::writeVce(Registers::Control value) -> void {
    m_vce = value;
}

auto Registers::readAccumulator(std::size_t index) -> uint64_t {
    return std::bit_cast<uint64_t>(m_accums[index]);
}

template <std::integral T>
auto Registers::writeAccumulator(std::size_t index, T value) -> void {
    m_accums[index] = std::bit_cast<Accumulator>(static_cast<int64_t>(value));
}

auto Registers::readAccumulators() -> std::array<Accumulator, 8> {
    return m_accums;
}

template <std::integral T>
auto Registers::writeAccumulators(VPR<T> vpr) -> void {
    for (auto i = 0uz; i < 8; ++i) {
        m_accums[i] = std::bit_cast<Accumulator>(static_cast<int64_t>(vpr[i]));
    }
}

} // namespace RSP