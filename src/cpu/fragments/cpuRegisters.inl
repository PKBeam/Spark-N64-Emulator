auto CPU::readPc() -> uint64_t {
    auto value = m_regs.pc;
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MAX>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "PC"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

auto CPU::writePc(uint64_t value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MAX>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "PC"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    m_regs.pc = value;
}

template <std::integral T>
auto CPU::readGpr(std::size_t index) -> T {
    auto value = static_cast<T>(m_regs.gprs[index]);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "{}", static_cast<ISA::CPU_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return index == 0 ? 0 : value;
}

template <ISA::CPU_REG R, std::integral T>
auto CPU::readGpr() -> T {
    return readGpr<T>(static_cast<std::size_t>(R));
}

template <std::integral T>
auto CPU::writeGpr(std::size_t index, T value) -> void {
    if (index != 0) {
        m_regs.gprs[index] = value;
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Util::Verbosity::HIGH>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "{}", static_cast<ISA::CPU_REG>(index)},
                std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
        }
    }
}

template <ISA::CPU_REG R, std::integral T>
auto CPU::writeGpr(T value) -> void {
    writeGpr<T>(static_cast<std::size_t>(R), value);
}

template <std::integral T>
auto CPU::readHi() -> T {
    auto value = static_cast<T>(m_regs.hi);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <std::integral T>
auto CPU::writeHi(T value) -> void {
    m_regs.hi = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <std::integral T>
auto CPU::readLo() -> T {
    auto value = static_cast<T>(m_regs.lo);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <std::integral T>
auto CPU::writeLo(T value) -> void {
    m_regs.lo = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}
