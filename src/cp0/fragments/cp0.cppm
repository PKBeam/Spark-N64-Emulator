module;

#include <util/defines.hpp>

export module CP0:CP0;

import std;
import ISA;
import Util;

export namespace CP0 {

struct Status {
    uint32_t ie  : 1;
    uint32_t exl : 1;
    uint32_t erl : 1 = 1;
    uint32_t ksu : 2;
    uint32_t ux  : 1;
    uint32_t sx  : 1;
    uint32_t kx  : 1;
    uint32_t im  : 8;
    // Diagnostic Status bits
    uint32_t de  : 1;
    uint32_t ce  : 1;
    uint32_t ch  : 1;
    uint32_t     : 1;
    uint32_t sr  : 1 = 0; // TODO set 1 on soft reset/NMI interrupt
    uint32_t ts  : 1 = 0;
    uint32_t bev : 1 = 1;
    uint32_t     : 1;
    uint32_t its : 1;
    //
    uint32_t re : 1;
    uint32_t fr : 1;
    uint32_t rp : 1 = 0;
    uint32_t cu : 4;
};

struct Cause {
    uint32_t     : 2;
    uint32_t exc : 5;
    uint32_t     : 1;
    uint32_t ip  : 8;
    uint32_t     : 12;
    uint32_t ce  : 2;
    uint32_t     : 1;
    uint32_t bd  : 1;
};

enum class ExceptionCode : uint8_t {
    INTERRUPT     = 0,
    TLB_MOD       = 1,
    TLB_LOAD      = 2,
    TLB_STORE     = 3,
    ADDR_LOAD     = 4,
    ADDR_STORE    = 5,
    INST_BUS_ERR  = 6,
    DATA_BUS_ERR  = 7,
    SYSCALL       = 8,
    BREAKPOINT    = 9,
    RESERVED_INST = 10,
    COP_UNUSABLE  = 11,
    OVERFLOW      = 12,
    TRAP          = 13,
    FLOAT         = 15,
    WATCH         = 23,
};

enum class Registers : uint8_t {
    INDEX                = 0,
    RANDOM               = 1,
    ENTRYLO0             = 2,
    ENTRYLO1             = 3,
    CONTEXT              = 4,
    PAGEMASK             = 5,
    WIRED                = 6,
    BADVADDR             = 8,
    COUNT                = 9,
    ENTRYHI              = 10,
    COMPARE              = 11,
    STATUS[[= ^^Status]] = 12,
    CAUSE[[= ^^Cause]]   = 13,
    EPC                  = 14,
    PRID                 = 15,
    CONFIG               = 16,
    LLADDR               = 17,
    WATCHLO              = 18,
    WATCHHI              = 19,
    XCONTEXT             = 20,
    PARITYERR            = 26,
    CACHEERR             = 27,
    TAGLO                = 28,
    TAGHI                = 29,
    ERROREPC             = 30,
};

class CP0 {
  public:
    CP0(std::shared_ptr<Util::Logger> logger);

    template <std::integral T = int32_t>
    auto readReg(std::size_t index) -> T;

    template <std::integral T = int32_t>
    auto readReg(Registers index) -> T {
        return readReg<T>(static_cast<uint8_t>(index));
    }

    template <Registers R>
    auto readReg();

    template <std::integral T>
    auto writeReg(std::size_t index, T value) -> void;

    template <typename T>
        requires(!std::integral<T>)
    auto writeReg(T value) -> void;

    template <Registers R, std::integral T>
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
    const auto regName = static_cast<Registers>(index);

    auto value = static_cast<T>(m_regs[index]);
    IF_LOG_ENABLED(m_logger) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "CP0 {}", static_cast<Registers>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <Registers R>
auto CP0::readReg() {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^Registers)) {
        if constexpr (std::meta::extract<Registers>(e) == R) {
            uint32_t value = readReg(static_cast<uint8_t>(std::meta::extract<Registers>(e)));
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
    const auto regName = static_cast<Registers>(index);
    if (m_logger && regName == Registers::RANDOM) {
        m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Attempted to write to read-only register CP0_REG::RANDOM!"});
        return;
    }
    if (m_logger && regName == Registers::STATUS) {
        auto status = std::bit_cast<Status>(static_cast<uint32_t>(value));
        if (status.kx || status.sx || status.ux) {
            m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Enabled 64-bit mode in CP0_REG::STATUS, which is not fully supported yet"});
        }
    }
    m_regs[index] = Util::signExt32(value);
    IF_LOG_ENABLED(m_logger) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "CP0 {}", static_cast<Registers>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    if (index == static_cast<uint8_t>(Registers::STATUS) || index == static_cast<uint8_t>(Registers::CAUSE)) {
        updateInterrupt();
    }
}

template <typename T>
    requires(!std::integral<T>)
auto CP0::writeReg(T value) -> void {
    template for (constexpr auto e : Util::staticEnumeratorsOf(^^Registers)) {
        if constexpr (std::meta::annotations_of(e).size() == 0) { // ICE if using Util::staticAnnotationsOf
            continue;
        } else if constexpr (^^T == Util::dealiasedTypeOf(Util::annotationOf(e))) {
            writeReg(static_cast<uint8_t>(std::meta::extract<Registers>(e)), std::bit_cast<uint32_t>(value));
        }
    }
}

template <Registers R, std::integral T>
auto CP0::writeReg(T value) -> void {
    writeReg(static_cast<uint8_t>(R), value);
}

auto CP0::updateInterrupt() -> void {
    auto status    = WITH_LOG_DISABLED(m_logger, readReg<Registers::STATUS>());
    auto cause     = WITH_LOG_DISABLED(m_logger, readReg<Registers::CAUSE>());
    m_hasInterrupt = status.im & cause.ip && status.ie && !status.exl && !status.erl;
}

auto CP0::clearInterrupt() -> void {
    m_hasInterrupt = false;
}

auto CP0::hasInterrupt() -> bool {
    return m_hasInterrupt;
}

} // namespace CP0

template <>
struct std::formatter<CP0::Registers> {
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator {
        return ctx.begin();
    }

    auto format(const CP0::Registers& reg, std::format_context& ctx) const {
        auto str = Util::enumName(reg).value_or(std::format("{}", static_cast<uint8_t>(reg)));
        return std::format_to(ctx.out(), "{}", str);
    }
};
