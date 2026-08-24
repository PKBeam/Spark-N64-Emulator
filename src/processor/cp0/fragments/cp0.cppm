module;

#include <util/defines.hpp>

export module CP0:CP0;

import std;
import ISA;
import Util;

export namespace CP0 {

class CP0 {
  public:
    CP0(std::shared_ptr<Util::Logger> logger);

    template <std::integral T = int32_t>
    auto readReg(std::size_t index) -> T;

    template <std::integral T = int32_t>
    auto readReg(ISA::CP0_REG index) -> T {
        return readReg<T>(static_cast<uint8_t>(index));
    }

    template <ISA::CP0_REG R>
    auto readReg();

    template <std::integral T>
    auto writeReg(std::size_t index, T value) -> void;

    template <typename T>
        requires(!std::integral<T>)
    auto writeReg(T value) -> void;

    template <ISA::CP0_REG R, std::integral T>
    auto writeReg(T value) -> void;

    auto updateInterrupt() -> void;
    auto clearInterrupt() -> void;

    auto hasInterrupt() -> bool;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    bool                          m_hasInterrupt = false;
    std::array<uint32_t, 32>      m_regs{};
};

CP0::CP0(std::shared_ptr<Util::Logger> logger) {
    m_logger = logger;
    m_regs   = {};
}

template <std::integral T>
auto CP0::readReg(std::size_t index) -> T {
    const auto regName = static_cast<ISA::CP0_REG>(index);

    auto value = static_cast<T>(m_regs[index]);
    IF_LOG_ENABLED(m_logger) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Level::HIGH, Sys::CPU>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "CP0 {}", static_cast<ISA::CP0_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <ISA::CP0_REG R>
auto CP0::readReg() {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^ISA::CP0_REG)) {
        if constexpr (std::meta::extract<ISA::CP0_REG>(e) == R) {
            uint32_t value = readReg(static_cast<uint8_t>(std::meta::extract<ISA::CP0_REG>(e)));
            if constexpr (std::meta::annotations_of(e).size() == 0) { // ICE if using Util::staticAnnotationsOf
                return value;
            } else {
                constexpr auto regType = Util::dealiasedTypeOf(Util::annotationOf(e));
                return std::bit_cast<typename[:regType:]>(value);
            }
        }
    }
}

template <std::integral T>
auto CP0::writeReg(std::size_t index, T value) -> void {
    const auto regName = static_cast<ISA::CP0_REG>(index);
    if (m_logger && regName == ISA::CP0_REG::RANDOM) {
        m_logger->log<Level::HIGH, Sys::CPU>(std::tuple{"warning", "Attempted to write to read-only register ISA::CP0_REG::RANDOM!"});
        return;
    }
    if (m_logger && regName == ISA::CP0_REG::STATUS) {
        auto status = std::bit_cast<ISA::CP0Status>(static_cast<uint32_t>(value));
        if (status.kx || status.sx || status.ux) {
            m_logger->log<Level::HIGH, Sys::CPU>(std::tuple{"warning", "Enabled 64-bit mode in ISA::CP0_REG::STATUS, which is not fully supported yet"});
        }
    }
    m_regs[index] = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Level::HIGH, Sys::CPU>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "CP0 {}", static_cast<ISA::CP0_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    if (index == static_cast<uint8_t>(ISA::CP0_REG::STATUS) || index == static_cast<uint8_t>(ISA::CP0_REG::CAUSE)) {
        updateInterrupt();
    }
}

template <typename T>
    requires(!std::integral<T>)
auto CP0::writeReg(T value) -> void {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^ISA::CP0_REG)) {
        if constexpr (std::meta::annotations_of(e).size() == 0) { // ICE if using Util::staticAnnotationsOf
            continue;
        } else if constexpr (^^T == Util::dealiasedTypeOf(Util::annotationOf(e))) {
            writeReg(static_cast<uint8_t>(std::meta::extract<ISA::CP0_REG>(e)), std::bit_cast<uint32_t>(value));
        }
    }
}

template <ISA::CP0_REG R, std::integral T>
auto CP0::writeReg(T value) -> void {
    writeReg(static_cast<uint8_t>(R), value);
}

auto CP0::updateInterrupt() -> void {
    auto status    = WITH_LOG_DISABLED(m_logger, readReg<ISA::CP0_REG::STATUS>());
    auto cause     = WITH_LOG_DISABLED(m_logger, readReg<ISA::CP0_REG::CAUSE>());
    m_hasInterrupt = status.im & cause.ip && status.ie && !status.exl && !status.erl;
}

auto CP0::clearInterrupt() -> void {
    m_hasInterrupt = false;
}

auto CP0::hasInterrupt() -> bool {
    return m_hasInterrupt;
}

} // namespace CP0
