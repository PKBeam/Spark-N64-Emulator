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
}; // namespace RSP

template <>
struct std::formatter<std::array<RSP::Accumulator, 8>> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const std::array<RSP::Accumulator, 8>& accums, std::format_context& ctx) const {
        auto out = ctx.out();
        for (const auto& [i, accum] : std::views::enumerate(accums)) {
            if (i > 0) {
                out = std::format_to(out, " ");
            }
            out = std::format_to(out, HEXFMT48, static_cast<uint64_t>(accum));
        }
        return out;
    }
};

export namespace RSP {

template <std::integral T>
    requires(sizeof(T) == 2)
struct VPR {
  private:
    constexpr auto getIndices(std::size_t byte) const -> std::pair<std::size_t, std::size_t> {
        if (byte >= Size * sizeof(T)) {
            throw Util::Error("VPR byte index {} out of range (max {})", byte, Size * sizeof(T));
        }
        const auto index     = byte / sizeof(T);
        const auto indexByte = Util::isLittleEndian() ? (sizeof(T) - 1) - (byte % sizeof(T)) : byte % sizeof(T);
        return {index, indexByte};
    }

  public:
    static constexpr auto Size = 8uz;

    template <typename Self>
    constexpr auto operator[](this Self&& self, std::size_t index) -> decltype(auto) {
        return std::forward<Self>(self).m_data[Size - 1 - index];
    }

    constexpr auto getByte(std::size_t index) const -> uint8_t {
        const auto [i, byte] = getIndices(index);
        return static_cast<uint8_t>((*this)[i] >> (byte * 8));
    }

    constexpr auto setByte(std::size_t index, uint8_t value) -> void {
        const auto [i, byte] = getIndices(index);
        (*this)[i] &= ~(0xFF << (byte * 8));
        (*this)[i] |= static_cast<T>(value) << (byte * 8);
    }

    constexpr auto size() const {
        return Size;
    }

    template <typename Self>
    constexpr auto begin(this Self&& self) {
        return self.m_data.rbegin();
    }

    template <typename Self>
    constexpr auto end(this Self&& self) {
        return self.m_data.rend();
    }

  private:
    std::array<T, Size> m_data{};
};

struct Registers {
    constexpr Registers(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {}

    template <std::integral T = uint16_t>
        requires(sizeof(T) == 2)
    constexpr auto readVpr(std::size_t index, ISA::VEC_ELEM elem = ISA::VEC_ELEM::NONE_0) const -> VPR<T>;

    template <std::integral T = uint16_t>
        requires(sizeof(T) == 2)
    constexpr auto writeVpr(std::size_t index, VPR<T> value, ISA::VEC_ELEM elem = ISA::VEC_ELEM::NONE_0) -> void;

    template <std::integral T>
        requires(sizeof(T) == 2)
    constexpr auto writeVpr(std::size_t index, T value, ISA::VEC_ELEM elem) -> void;

    constexpr auto readVcc() const -> std::bitset<16>;
    constexpr auto writeVcc(std::bitset<16> value) -> void;
    constexpr auto clearVcc() -> void;

    constexpr auto readVco() const -> std::bitset<16>;
    constexpr auto writeVco(std::bitset<16> value) -> void;
    constexpr auto clearVco() -> void;

    constexpr auto readVce() const -> std::bitset<8>;
    constexpr auto writeVce(std::bitset<8> value) -> void;
    constexpr auto clearVce() -> void;

    constexpr auto readDivIn() const -> std::optional<uint32_t>;
    constexpr auto writeDivIn(std::optional<uint32_t> value) -> void;

    constexpr auto readDivOut() const -> uint32_t;
    constexpr auto writeDivOut(uint32_t value) -> void;

    constexpr auto readAccumulators() const -> std::array<Accumulator, 8>;
    constexpr auto writeAccumulators(std::array<Accumulator, 8> value) -> void;

    template <std::integral T>
    constexpr auto setAccumulators(VPR<T> vpr) -> void;

    template <std::integral T>
    constexpr auto addToAccumulators(VPR<T> vpr) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;

    std::array<VPR<uint16_t>, 32> m_vprs;
    std::array<Accumulator, 8>    m_accums;
    std::bitset<16>               m_vcc;
    std::bitset<16>               m_vco;
    std::bitset<8>                m_vce;
    std::optional<uint32_t>       m_divIn;
    uint32_t                      m_divOut{};

    template <std::meta::info vecElem>
    consteval auto getLanesForElement() const -> std::array<std::size_t, 8>;
};

template <std::integral T>
    requires(sizeof(T) == 2)
constexpr auto Registers::readVpr(std::size_t index, ISA::VEC_ELEM elem) const -> VPR<T> {
    VPR<T> result{};
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^ISA::VEC_ELEM)) {
        if (elem == std::meta::extract<ISA::VEC_ELEM>(e)) {
            constexpr auto lanes = getLanesForElement<e>();
            template for (constexpr auto i : std::views::iota(0uz, lanes.size())) {
                result[i] = std::bit_cast<T>(m_vprs[index][lanes[i]]);
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::MED, Sys::RSP>(
                    std::tuple{"op", "r"},
                    std::tuple{std::format("$v{}", index), "{:n:#06x}", std::bit_cast<VPR<std::make_unsigned_t<T>>>(result)});
            }
            return result;
        }
    }
    throw Util::Error("Invalid element value {}", static_cast<int>(elem));
}

template <std::integral T>
    requires(sizeof(T) == 2)
constexpr auto Registers::writeVpr(std::size_t index, VPR<T> value, ISA::VEC_ELEM elem) -> void {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^ISA::VEC_ELEM)) {
        if (elem == std::meta::extract<ISA::VEC_ELEM>(e)) {
            constexpr auto lanes = getLanesForElement<e>();
            template for (constexpr auto i : std::views::iota(0uz, lanes.size())) {
                m_vprs[index][lanes[i]] = std::bit_cast<uint16_t>(value[i]);
            }
            IF_LOG_ENABLED(m_logger) {
                m_logger->log<Level::MED, Sys::RSP>(
                    std::tuple{"op", "w"},
                    std::tuple{std::format("$v{}", index), "{:n:#06x}", std::bit_cast<VPR<std::make_unsigned_t<T>>>(value)});
            }
        }
    }
}

template <std::integral T>
    requires(sizeof(T) == 2)
constexpr auto Registers::writeVpr(std::size_t index, T value, ISA::VEC_ELEM elem) -> void {
    auto values = VPR<T>{};
    std::fill_n(values.begin(), values.size(), value);
    writeVpr(index, values, elem);
}

constexpr auto Registers::readVcc() const -> std::bitset<16> {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "r"},
            std::tuple{"VCC", HEXFMT16, static_cast<uint16_t>(m_vcc.to_ulong())});
    }
    return m_vcc;
}

constexpr auto Registers::writeVcc(std::bitset<16> value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "w"},
            std::tuple{"VCC", HEXFMT16, static_cast<uint16_t>(value.to_ulong())});
    }
    m_vcc = value;
}

constexpr auto Registers::clearVcc() -> void {
    writeVcc(std::bitset<16>{});
}

constexpr auto Registers::readVco() const -> std::bitset<16> {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "r"},
            std::tuple{"VCO", HEXFMT16, static_cast<uint16_t>(m_vco.to_ulong())});
    }
    return m_vco;
}

constexpr auto Registers::writeVco(std::bitset<16> value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "w"},
            std::tuple{"VCO", HEXFMT16, static_cast<uint16_t>(value.to_ulong())});
    }
    m_vco = value;
}

constexpr auto Registers::clearVco() -> void {
    writeVco(std::bitset<16>{});
}

constexpr auto Registers::readVce() const -> std::bitset<8> {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "r"},
            std::tuple{"VCE", HEXFMT8, static_cast<uint8_t>(m_vce.to_ulong())});
    }
    return m_vce;
}

constexpr auto Registers::writeVce(std::bitset<8> value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "w"},
            std::tuple{"VCE", HEXFMT8, static_cast<uint8_t>(value.to_ulong())});
    }
    m_vce = value;
}

constexpr auto Registers::clearVce() -> void {
    writeVce(std::bitset<8>{});
}

constexpr auto Registers::readDivIn() const -> std::optional<uint32_t> {
    IF_LOG_ENABLED(m_logger) {
        if (m_divIn) {
            m_logger->log<Level::MED, Sys::RSP>(
                std::tuple{"op", "r"},
                std::tuple{"DIV_IN", HEXFMT32, *m_divIn});
        } else {
            m_logger->log<Level::MED, Sys::RSP>(
                std::tuple{"op", "r"},
                std::tuple{"DIV_IN", "NULL"});
        }
    }
    return m_divIn;
}

constexpr auto Registers::writeDivIn(std::optional<uint32_t> value) -> void {
    IF_LOG_ENABLED(m_logger) {
        if (value) {
            m_logger->log<Level::MED, Sys::RSP>(
                std::tuple{"op", "w"},
                std::tuple{"DIV_IN", HEXFMT32, *value});
        } else {
            m_logger->log<Level::MED, Sys::RSP>(
                std::tuple{"op", "w"},
                std::tuple{"DIV_IN", "NULL"});
        }
    }
    m_divIn = value;
}

constexpr auto Registers::readDivOut() const -> uint32_t {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "r"},
            std::tuple{"DIV_OUT", HEXFMT32, m_divOut});
    }
    return m_divOut;
}

constexpr auto Registers::writeDivOut(uint32_t value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "w"},
            std::tuple{"DIV_OUT", HEXFMT32, value});
    }
    m_divOut = value;
}

constexpr auto Registers::readAccumulators() const -> std::array<Accumulator, 8> {
    const auto value = m_accums;
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "r"},
            std::tuple{"ACC", "{}", value});
    }
    return value;
}

constexpr auto Registers::writeAccumulators(std::array<Accumulator, 8> value) -> void {
    m_accums = value;
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, Sys::RSP>(
            std::tuple{"op", "w"},
            std::tuple{"ACC", "{}", value});
    }
}

template <std::integral T>
constexpr auto Registers::setAccumulators(VPR<T> vpr) -> void {
    using Int64_Type = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;

    auto result = std::array<Accumulator, 8>{};
    for (auto i = 0uz; i < 8; ++i) {
        result[i] = Accumulator(static_cast<Int64_Type>(vpr[i]));
    }
    writeAccumulators(result);
}

template <std::integral T>
constexpr auto Registers::addToAccumulators(VPR<T> vpr) -> void {
    using Int64_Type = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;

    const auto accums = readAccumulators();
    auto       result = std::array<Accumulator, 8>{};
    for (auto i = 0uz; i < 8; ++i) {
        const auto value = static_cast<Int64_Type>(accums[i]) + static_cast<Int64_Type>(vpr[i]);
        result[i]        = Accumulator(value);
    }
    writeAccumulators(result);
}

template <std::meta::info vecElem>
consteval auto Registers::getLanesForElement() const -> std::array<std::size_t, 8> {
    auto result = std::array<std::size_t, 8>{};
    template for (auto i = 0; constexpr auto a : Util::staticAnnotationsOf(vecElem)) {
        result[i++] = std::meta::extract<int>(a);
    }
    return result;
}

} // namespace RSP
