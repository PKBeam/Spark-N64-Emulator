auto VR4300::readPc() -> uint64_t {
    auto value = m_regs.pc;
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MAX>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "PC"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

auto VR4300::writePc(uint64_t value) -> void {
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MAX>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "PC"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    m_regs.pc = value;
}

template <std::integral T>
auto VR4300::readGpr(std::size_t index) -> T {
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
auto VR4300::readGpr() -> T {
    return readGpr<T>(static_cast<std::size_t>(R));
}

template <std::integral T>
auto VR4300::writeGpr(std::size_t index, T value) -> void {
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
auto VR4300::writeGpr(T value) -> void {
    writeGpr<T>(static_cast<std::size_t>(R), value);
}

template <std::integral T>
auto VR4300::readHi() -> T {
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
auto VR4300::writeHi(T value) -> void {
    m_regs.hi = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <std::integral T>
auto VR4300::readLo() -> T {
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
auto VR4300::writeLo(T value) -> void {
    m_regs.lo = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::HIGH>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <std::integral T>
auto VR4300::readCp0Reg(std::size_t index) -> T {
    const auto regName = static_cast<ISA::CP0_REG>(index);

    auto value = static_cast<T>(m_cp0regs[index]);
    IF_LOG_ENABLED(m_logger) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "CP0 {}", static_cast<ISA::CP0_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <ISA::CP0_REG R, std::integral T>
auto VR4300::readCp0Reg() -> T {
    return readCp0Reg(static_cast<std::size_t>(R));
}

template <std::integral T>
auto VR4300::writeCp0Reg(std::size_t index, T value) -> void {
    const auto regName = static_cast<ISA::CP0_REG>(index);
    if (m_logger && regName == ISA::CP0_REG::RANDOM) {
        m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Attempted to write to read-only register CP0_REG::RANDOM!"});
        return;
    }
    if (m_logger && regName == ISA::CP0_REG::STATUS) {
        auto status = std::bit_cast<CP0_STATUS>(static_cast<uint32_t>(value));
        if (status.kx || status.sx || status.ux) {
            m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Enabled 64-bit mode in CP0_REG::STATUS, which is not fully supported yet"});
        }
    }
    m_cp0regs[index] = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "CP0 {}", static_cast<ISA::CP0_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <ISA::CP0_REG R, std::integral T>
auto VR4300::writeCp0Reg(T value) -> void {
    writeCp0Reg(static_cast<std::size_t>(R), value);
}
