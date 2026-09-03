module;
#include <util/defines.hpp>
export module CPU:Registers;

import std;
import ISA;
import Util;

export namespace CPU {

template <Sys System>
struct Registers {

    Registers(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {}

    auto readPc() const -> uint64_t;
    auto writePc(uint64_t value) -> void;
    auto writePcDelayed(uint64_t newPc) -> void;
    auto pcIsDelaySlot() const -> bool;
    auto clearDelaySlot() -> void;
    auto advancePc() -> void;
    auto getNextPc() const -> uint64_t;

    template <std::integral T = int32_t>
    auto readGpr(std::size_t index) const -> T;

    template <ISA::CPU_REG R, std::integral T = int32_t>
    auto readGpr() const -> T {
        return readGpr<T>(static_cast<std::size_t>(R));
    }

    template <std::integral T>
    auto writeGpr(std::size_t index, T value) -> void;

    template <ISA::CPU_REG R, std::integral T>
    auto writeGpr(T value) -> void {
        writeGpr<T>(static_cast<std::size_t>(R), value);
    }

    template <std::integral T = int64_t>
    auto readHi() const -> T;

    template <std::integral T>
    auto writeHi(T value) -> void;

    template <std::integral T = int64_t>
    auto readLo() const -> T;

    template <std::integral T>
    auto writeLo(T value) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;

    std::array<uint64_t, 32> m_gprs;
    uint64_t                 m_hi{};
    uint64_t                 m_lo{};

    // program counter state management
    uint64_t                m_pc{};
    std::optional<uint64_t> m_delaySlotPc;
    std::optional<uint64_t> m_pendingJumpPc;
};

template <Sys System>
auto Registers<System>::readPc() const -> uint64_t {
    return m_pc;
}

template <Sys System>
auto Registers<System>::writePc(uint64_t value) -> void {
    m_pc = value;
}

template <Sys System>
auto Registers<System>::writePcDelayed(uint64_t newPc) -> void {
    m_pendingJumpPc = newPc;
}

template <Sys System>
auto Registers<System>::pcIsDelaySlot() const -> bool {
    return m_delaySlotPc.has_value();
}

template <Sys System>
auto Registers<System>::clearDelaySlot() -> void {
    m_delaySlotPc.reset();
}

template <Sys System>
auto Registers<System>::advancePc() -> void {
    if (m_delaySlotPc) {
        m_pc = *m_delaySlotPc;
        m_delaySlotPc.reset();
    } else {
        m_pc += 4;
    }
    if constexpr (System == Sys::RSP) {
        m_pc &= 0xFFF;
    }
    if (m_pendingJumpPc) {
        m_delaySlotPc = m_pendingJumpPc;
        m_pendingJumpPc.reset();
    }
}

template <Sys System>
auto Registers<System>::getNextPc() const -> uint64_t {
    if (m_delaySlotPc) {
        return *m_delaySlotPc;
    }
    return m_pc + 4;
}

template <Sys System>
template <std::integral T>
auto Registers<System>::readGpr(std::size_t index) const -> T {
    auto value = static_cast<T>(m_gprs[index]);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, System>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "{}", static_cast<ISA::CPU_REG>(index)},
            std::tuple{"data", HEXFMT32, static_cast<uint32_t>(value)});
    }
    return index == 0 ? 0 : value;
}

template <Sys System>
template <std::integral T>
auto Registers<System>::writeGpr(std::size_t index, T value) -> void {
    if (index != 0) {
        m_gprs[index] = value;
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Level::MED, System>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "{}", static_cast<ISA::CPU_REG>(index)},
                std::tuple{"data", HEXFMT32, static_cast<uint32_t>(value)});
        }
    }
}

template <Sys System>
template <std::integral T>
auto Registers<System>::readHi() const -> T {
    auto value = static_cast<T>(m_hi);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, System>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", HEXFMT32, static_cast<uint32_t>(value)});
    }
    return value;
}

template <Sys System>
template <std::integral T>
auto Registers<System>::writeHi(T value) -> void {
    m_hi = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, System>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", HEXFMT32, static_cast<uint32_t>(value)});
    }
}

template <Sys System>
template <std::integral T>
auto Registers<System>::readLo() const -> T {
    auto value = static_cast<T>(m_lo);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, System>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", HEXFMT32, static_cast<uint32_t>(value)});
    }
    return value;
}

template <Sys System>
template <std::integral T>
auto Registers<System>::writeLo(T value) -> void {
    m_lo = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::MED, System>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", HEXFMT32, static_cast<uint32_t>(value)});
    }
}

} // namespace CPU