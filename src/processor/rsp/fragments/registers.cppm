module;

#include <util/defines.hpp>

export module RSP:Registers;

import std;
import ISA;
import Util;

export namespace RSP {
struct Accumulator {
    uint16_t low  = 0;
    uint16_t mid  = 0;
    uint16_t high = 0;

    template <std::integral T>
        requires(sizeof(T) == 8)
    constexpr Accumulator(T value) {
        low  = value & 0xFFFF;
        mid  = (value >> 16) & 0xFFFF;
        high = (value >> 32) & 0xFFFF;
    }

    constexpr Accumulator() {
        low  = 0;
        mid  = 0;
        high = 0;
    }

    // note: accumulator is 48 bits so it can always fit in an int64_t without overflow
    constexpr explicit operator int64_t() const {
        const auto low  = static_cast<int64_t>(this->low);
        const auto mid  = static_cast<int64_t>(this->mid);
        const auto high = static_cast<int64_t>(this->high);
        const auto top  = static_cast<int64_t>((high >> 15) ? 0xFFFF : 0); // sign ext 48 bit
        return (top << 48) | (high << 32) | (mid << 16) | low;
    }

    constexpr explicit operator uint64_t() const {
        return static_cast<uint64_t>(static_cast<int64_t>(*this));
    }

    constexpr auto operator=(int64_t value) {
        *this = Accumulator(value);
        return *this;
    }

    template <std::integral T>
    constexpr auto clamped() const -> T {
        const auto     value = static_cast<int64_t>(*this);
        constexpr auto min   = static_cast<int64_t>(std::numeric_limits<T>::min());
        constexpr auto max   = static_cast<int64_t>(std::numeric_limits<T>::max());
        return std::clamp(value, min, max);
    }
};

struct ControlReg {
    uint8_t low  = 0;
    uint8_t high = 0;

    constexpr ControlReg() {
        low  = 0;
        high = 0;
    }

    constexpr ControlReg(uint16_t value) {
        low  = value & 0xFF;
        high = (value >> 8) & 0xFF;
    }

    constexpr explicit operator uint16_t() const {
        return (static_cast<uint8_t>(high) << 8) | static_cast<uint8_t>(low);
    }
};

}; // namespace RSP

template <>
struct std::formatter<std::array<RSP::Accumulator, 8>> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const std::array<RSP::Accumulator, 8>& accums, std::format_context& ctx) const {
        auto out = ctx.out();
        for (const auto& accum : accums) {
            out = std::format_to(out, "{:#014x} ", static_cast<uint64_t>(accum));
        }
        return out;
    }
};

export namespace RSP {
struct Registers {
    template <std::integral T>
        requires(sizeof(T) == 2)
    using VPR = std::array<T, 8>;

    Registers(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {}

    auto getVprData(std::size_t index) -> std::byte*;

    template <std::integral T = uint16_t>
        requires(sizeof(T) == 2)
    auto readVpr(std::size_t index, ISA::VEC_ELEM elem = ISA::VEC_ELEM::NONE_0) -> VPR<T>;

    template <std::integral T = uint16_t>
        requires(sizeof(T) == 2)
    auto writeVpr(std::size_t index, VPR<T> value, ISA::VEC_ELEM elem = ISA::VEC_ELEM::NONE_0) -> void;

    template <std::integral T>
        requires(sizeof(T) == 2)
    auto writeVpr(std::size_t index, T value, ISA::VEC_ELEM elem) -> void;

    auto readVcc() -> ControlReg;
    auto writeVcc(ControlReg value) -> void;

    auto readVco() -> ControlReg;
    auto writeVco(ControlReg value) -> void;

    auto readVce() -> ControlReg;
    auto writeVce(ControlReg value) -> void;

    auto readDivIn() -> std::optional<uint32_t>;
    auto writeDivIn(std::optional<uint32_t> value) -> void;

    auto readDivOut() -> uint32_t;
    auto writeDivOut(uint32_t value) -> void;

    auto readAccumulators() -> std::array<Accumulator, 8>;
    auto writeAccumulators(std::array<Accumulator, 8> value) -> void;

    template <std::integral T>
    auto setAccumulators(VPR<T> vpr) -> void;

    template <std::integral T>
    auto addToAccumulators(VPR<T> vpr) -> void;

  private:
    auto readAccumulator(std::size_t index) -> uint64_t;

    template <std::integral T>
    auto writeAccumulator(std::size_t index, T value) -> void;

    std::shared_ptr<Util::Logger> m_logger;

    std::array<VPR<uint16_t>, 32> m_vprs{};
    std::array<Accumulator, 8>    m_accums{};
    RSP::ControlReg               m_vcc{};
    RSP::ControlReg               m_vco{};
    RSP::ControlReg               m_vce{};
    std::optional<uint32_t>       m_divIn{};
    uint32_t                      m_divOut{};

    template <std::meta::info vecElem>
    consteval auto getLanesForElement() {
        auto result = std::array<int, 8>{};
        template for (auto i = 0; constexpr auto a : Util::staticAnnotationsOf(vecElem)) {
            result[i++] = std::meta::extract<int>(a);
        }
        return result;
    }
};

auto Registers::getVprData(std::size_t index) -> std::byte* {
    return reinterpret_cast<std::byte*>(m_vprs[index].data());
}

template <std::integral T>
    requires(sizeof(T) == 2)
auto Registers::readVpr(std::size_t index, ISA::VEC_ELEM elem) -> VPR<T> {
    VPR<T> result{};
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^ISA::VEC_ELEM)) {
        if (elem == std::meta::extract<ISA::VEC_ELEM>(e)) {
            constexpr auto lanes = getLanesForElement<e>();
            template for (constexpr auto i : std::views::iota(0uz, lanes.size())) {
                result[i] = std::bit_cast<T>(m_vprs[index][lanes[i]]);
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::MED, Sys::RSP>(
                    std::tuple{"op", "read"},
                    std::tuple{"reg", "v{}", index},
                    std::tuple{"data", "{:n:#06x}", std::bit_cast<VPR<std::make_unsigned_t<T>>>(result)},
                    std::tuple{"lanes", "[{:n}]", lanes});
            }
            return result;
        }
    }
    throw Util::Error("Invalid element value {}", static_cast<int>(elem));
}

template <std::integral T>
    requires(sizeof(T) == 2)
auto Registers::writeVpr(std::size_t index, VPR<T> value, ISA::VEC_ELEM elem) -> void {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^ISA::VEC_ELEM)) {
        if (elem == std::meta::extract<ISA::VEC_ELEM>(e)) {
            constexpr auto lanes = getLanesForElement<e>();
            template for (constexpr auto i : std::views::iota(0uz, lanes.size())) {
                m_vprs[index][lanes[i]] = std::bit_cast<uint16_t>(value[i]);
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::MED, Sys::RSP>(
                    std::tuple{"op", "write"},
                    std::tuple{"reg", "v{}", index},
                    std::tuple{"data", "{:n:#06x}", std::bit_cast<VPR<std::make_unsigned_t<T>>>(value)},
                    std::tuple{"lanes", "[{:n}]", lanes});
            }
        }
    }
}

template <std::integral T>
    requires(sizeof(T) == 2)
auto Registers::writeVpr(std::size_t index, T value, ISA::VEC_ELEM elem) -> void {
    auto values = VPR<T>{};
    std::fill_n(values.begin(), values.size(), value);
    writeVpr(index, values, elem);
}

auto Registers::readVcc() -> RSP::ControlReg {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "VCC"},
            std::tuple{"data", "{:#06x}", static_cast<uint16_t>(m_vcc)});
    }
    return m_vcc;
}

auto Registers::writeVcc(RSP::ControlReg value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "VCC"},
            std::tuple{"data", "{:#06x}", static_cast<uint16_t>(value)});
    }
    m_vcc = value;
}

auto Registers::readVco() -> RSP::ControlReg {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "VCO"},
            std::tuple{"data", "{:#06x}", static_cast<uint16_t>(m_vco)});
    }
    return m_vco;
}

auto Registers::writeVco(RSP::ControlReg value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "VCO"},
            std::tuple{"data", "{:#06x}", static_cast<uint16_t>(value)});
    }
    m_vco = value;
}

auto Registers::readVce() -> RSP::ControlReg {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "VCE"},
            std::tuple{"data", "{:#06x}", static_cast<uint16_t>(m_vce)});
    }
    return m_vce;
}

auto Registers::writeVce(RSP::ControlReg value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "VCE"},
            std::tuple{"data", "{:#06x}", static_cast<uint16_t>(value)});
    }
    m_vce = value;
}

auto Registers::readDivIn() -> std::optional<uint32_t> {
    IF_LOG_ENABLED(m_logger) {
        if (m_divIn) {
            m_logger->log<Level::MED, Sys::RSP>(
                std::tuple{"op", "read"},
                std::tuple{"reg", "DIV_IN"},
                std::tuple{"data", "{:#010x}", *m_divIn});
        } else {
            m_logger->log<Level::MED, Sys::RSP>(
                std::tuple{"op", "read"},
                std::tuple{"reg", "DIV_IN"},
                std::tuple{"data", "NULL"});
        }
    }
    return m_divIn;
}

auto Registers::writeDivIn(std::optional<uint32_t> value) -> void {
    IF_LOG_ENABLED(m_logger) {
        if (value) {
            m_logger->log<Level::MED, Sys::RSP>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "DIV_IN"},
                std::tuple{"data", "{:#010x}", *value});
        } else {
            m_logger->log<Level::MED, Sys::RSP>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "DIV_IN"},
                std::tuple{"data", "NULL"});
        }
    }
    m_divIn = value;
}

auto Registers::readDivOut() -> uint32_t {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "DIV_OUT"},
            std::tuple{"data", "{:#010x}", m_divOut});
    }
    return m_divOut;
}

auto Registers::writeDivOut(uint32_t value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "DIV_OUT"},
            std::tuple{"data", "{:#010x}", value});
    }
    m_divOut = value;
}

auto Registers::readAccumulator(std::size_t index) -> uint64_t {
    return std::bit_cast<uint64_t>(static_cast<int64_t>(m_accums[index]));
}

template <std::integral T>
auto Registers::writeAccumulator(std::size_t index, T value) -> void {
    m_accums[index] = Accumulator(static_cast<int64_t>(value));
}

auto Registers::readAccumulators() -> std::array<Accumulator, 8> {
    const auto value = m_accums;
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "ACC"},
            std::tuple{"data", "{}", value});
    }
    return value;
}

auto Registers::writeAccumulators(std::array<Accumulator, 8> value) -> void {
    m_accums = value;
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "ACC"},
            std::tuple{"data", "{}", value});
    }
}

template <std::integral T>
auto Registers::setAccumulators(VPR<T> vpr) -> void {
    using Int64_Type = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;

    auto result = std::array<Accumulator, 8>{};
    for (auto i = 0uz; i < 8; ++i) {
        result[i] = Accumulator(static_cast<Int64_Type>(vpr[i]));
    }
    writeAccumulators(result);
}

template <std::integral T>
auto Registers::addToAccumulators(VPR<T> vpr) -> void {
    using Int64_Type = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;

    const auto accums = readAccumulators();
    auto       result = std::array<Accumulator, 8>{};
    for (auto i = 0uz; i < 8; ++i) {
        const auto value = static_cast<Int64_Type>(accums[i]) + static_cast<Int64_Type>(vpr[i]);
        result[i]        = Accumulator(value);
    }
    writeAccumulators(result);
}

} // namespace RSP
