
module;
#include <cfenv>
#include <util/defines.hpp>
export module CP1:Registers;

import std;
import std.compat;
import ISA;
import Util;

export namespace CP1 {

template <typename T>
concept FloatType_c = std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>;

struct Registers {
    enum class Mode : bool {
        FPRS_16, // FR == 0
        FPRS_32, // FR == 1
    };

    Registers(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {}

    constexpr auto setMode(::CP1::Registers::Mode mode) -> void;

    template <typename T>
        requires(FloatType_c<T>)
    constexpr auto readFpr(std::size_t index) const -> T; //
                                                          // pre(m_mode == Mode::FPRS_32 || (index % 2 == 0));

    constexpr auto readFpr(std::size_t index, ISA::CP1_FORMAT fmt) const -> std::variant<float, double, uint32_t, uint64_t>;

    template <typename T>
        requires(FloatType_c<T>)
    constexpr auto writeFpr(std::size_t index, T value) -> void; //
                                                                 // pre(m_mode == Mode::FPRS_32 || (index % 2 == 0));

    template <std::integral T>
    constexpr auto readFgr(std::size_t index) const -> T;

    template <std::integral T>
    constexpr auto writeFgr(std::size_t index, T value) -> void;

    constexpr auto readStatus() const -> ISA::CP1Status;
    constexpr auto writeStatus(uint32_t value) -> void;
    constexpr auto writeStatus(ISA::CP1Status value) -> void;

    constexpr auto readRevision() const -> ISA::CP1Revision;

    constexpr auto getRoundingMode() const -> decltype(FE_TOWARDZERO);

  private:
    std::shared_ptr<Util::Logger> m_logger;
    Mode                          m_mode{};
    std::array<uint64_t, 32>      m_fgrs{};
    ISA::CP1Revision              m_fcr0{};
    ISA::CP1Status                m_fcr31{};
};

constexpr auto Registers::setMode(::CP1::Registers::Mode mode) -> void {
    m_mode = mode;
}

constexpr auto Registers::readStatus() const -> ISA::CP1Status {
    return m_fcr31;
}

constexpr auto Registers::writeStatus(uint32_t value) -> void {
    m_fcr31 = std::bit_cast<ISA::CP1Status>(value);
}

constexpr auto Registers::writeStatus(ISA::CP1Status value) -> void {
    writeStatus(std::bit_cast<uint32_t>(value));
}

constexpr auto Registers::readRevision() const -> ISA::CP1Revision {
    return m_fcr0;
}

template <typename T>
    requires(FloatType_c<T>)
constexpr auto Registers::readFpr(std::size_t index) const -> T {
    const auto value = [this, index]() -> T {
        switch (m_mode) {
            case Mode::FPRS_16:
                if constexpr (sizeof(T) == 4) {
                    return std::bit_cast<T>(static_cast<uint32_t>(m_fgrs[index]));
                } else /* 64 bit */ {
                    return std::bit_cast<T>((m_fgrs[index / 2 + 1] << 32) | m_fgrs[index / 2]);
                }
            case Mode::FPRS_32:
                if constexpr (sizeof(T) == 4) {
                    return std::bit_cast<T>(static_cast<uint32_t>(m_fgrs[index]));
                } else {
                    return std::bit_cast<T>(m_fgrs[index]);
                }
        }
    }();
    IF_LOG_ENABLED(m_logger) {
        if constexpr (sizeof(T) == 4) {
            m_logger->log<Level::MED, Sys::CP1>(
                std::tuple{"op", "read"},
                std::tuple{"reg", "$f{}", index},
                std::tuple{"data", "{:#010X}", std::bit_cast<uint32_t>(value)});
        } else /* 64 bit */ {
            m_logger->log<Level::MED, Sys::CP1>(
                std::tuple{"op", "read"},
                std::tuple{"reg", "$f{}", index},
                std::tuple{"data", "{:#018X}", std::bit_cast<uint64_t>(value)});
        }
    }
    return value;
}

constexpr auto Registers::readFpr(std::size_t index, ISA::CP1_FORMAT fmt) const -> std::variant<float, double, uint32_t, uint64_t> {
    switch (fmt) {
        case ISA::CP1_FORMAT::S: return readFpr<float>(index);
        case ISA::CP1_FORMAT::D: return readFpr<double>(index);
        case ISA::CP1_FORMAT::W: return readFpr<uint32_t>(index);
        case ISA::CP1_FORMAT::L: return readFpr<uint64_t>(index);
        default: throw Util::Error("Invalid CP1 float format {}", static_cast<int>(fmt));
    }
}

template <typename T>
    requires(FloatType_c<T>)
constexpr auto Registers::writeFpr(std::size_t index, T value) -> void {
    switch (m_mode) {
        case Mode::FPRS_16:
            if constexpr (sizeof(T) == 4) {
                m_fgrs[index] = std::bit_cast<uint32_t>(value);
            } else /* 64 bit */ {
                m_fgrs[index / 2]     = static_cast<uint32_t>(std::bit_cast<uint64_t>(value) & 0xFFFFFFFF);
                m_fgrs[index / 2 + 1] = static_cast<uint32_t>(std::bit_cast<uint64_t>(value) >> 32);
            }
            break;
        case Mode::FPRS_32:
            if constexpr (sizeof(T) == 4) {
                m_fgrs[index] = std::bit_cast<uint32_t>(value);
            } else {
                m_fgrs[index] = std::bit_cast<uint64_t>(value);
            }
            break;
    }
    IF_LOG_ENABLED(m_logger) {
        if constexpr (sizeof(T) == 4) {
            m_logger->log<Level::MED, Sys::CP1>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "$f{}", index},
                std::tuple{"data", "{:#010X}", std::bit_cast<uint32_t>(value)});
        } else /* 64 bit */ {
            m_logger->log<Level::MED, Sys::CP1>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "$f{}", index},
                std::tuple{"data", "{:#018X}", std::bit_cast<uint64_t>(value)});
        }
    }
}

template <std::integral T>
constexpr auto Registers::readFgr(std::size_t index) const -> T {
    const auto value = [this, index]() -> T {
        switch (m_mode) {
            case Mode::FPRS_16:
                if constexpr (sizeof(T) == 4) {
                    if (index % 2 == 0) {
                        return static_cast<T>(m_fgrs[index]);
                    } else {
                        return static_cast<T>(m_fgrs[index - 1] >> 32);
                    }
                } else {
                    // index % 2 should be 0 here
                    return (m_fgrs[index + 1] << 32) | (m_fgrs[index] & 0xFFFFFFFF);
                }
            case Mode::FPRS_32:
                return static_cast<T>(m_fgrs[index]);
        }
    }();
    IF_LOG_ENABLED(m_logger) {
        if constexpr (sizeof(T) == 4) {
            m_logger->log<Level::MED, Sys::CP1>(
                std::tuple{"op", "read"},
                std::tuple{"reg", "$f{}", index},
                std::tuple{"data", "{:#010X}", std::bit_cast<uint32_t>(value)});
        } else {
            m_logger->log<Level::MED, Sys::CP1>(
                std::tuple{"op", "read"},
                std::tuple{"reg", "$f{}", index},
                std::tuple{"data", "{:#018X}", std::bit_cast<uint64_t>(value)});
        }
    }
    return Util::signExt32(value);
}

template <std::integral T>
constexpr auto Registers::writeFgr(std::size_t index, T value) -> void {
    if (m_mode == Mode::FPRS_16) {
        if constexpr (sizeof(T) == 4) {
            m_fgrs[index] = value & 0xFFFFFFFF;
        } else {
            // index % 2 should be 0 here
            m_fgrs[index + 1] = (value >> 32) & 0xFFFFFFFF;
            m_fgrs[index]     = value & 0xFFFFFFFF;
        }
    } else {
        m_fgrs[index] = value;
    }
    IF_LOG_ENABLED(m_logger) {
        if constexpr (sizeof(T) == 4) {
            m_logger->log<Level::MED, Sys::CP1>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "$f{}", index},
                std::tuple{"data", "{:#010X}", std::bit_cast<uint32_t>(value)});
        } else {
            m_logger->log<Level::MED, Sys::CP1>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "$f{}", index},
                std::tuple{"data", "{:#018X}", std::bit_cast<uint64_t>(value)});
        }
    }
}

constexpr auto Registers::getRoundingMode() const -> decltype(FE_TOWARDZERO) {
    switch (static_cast<ISA::CP1_ROUND_MODE>(m_fcr31.rm)) {
        case ISA::CP1_ROUND_MODE::RN: return FE_TONEAREST;
        case ISA::CP1_ROUND_MODE::RZ: return FE_TOWARDZERO;
        case ISA::CP1_ROUND_MODE::RP: return FE_UPWARD;
        case ISA::CP1_ROUND_MODE::RM: return FE_DOWNWARD;
    }
    throw Util::Error("Invalid CP1 rounding mode {}", static_cast<uint8_t>(m_fcr31.rm));
}

} // namespace CP1