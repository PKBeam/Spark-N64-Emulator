module;

#include <util/defines.hpp>

export module CPU:CPU;

import std;
import CP0;
import ISA;
import Memory;
import Util;

// clang-format off
namespace Shift {
enum Type : bool { LOGICAL, ARITHMETIC };
enum Len  : bool { WORD, DOUBLE };
enum Dir  : bool { LEFT, RIGHT };
enum Add  : bool { ADD_NONE, ADD32 };
enum Var  : bool { FIXED, VARIABLE };
}

namespace Immediate {
enum Extend : uint8_t { NONE, SIGN_EXTEND, ZERO_EXTEND };
}

namespace Branch {
enum Likelihood : bool { NONE, LIKELY };
enum Link : bool { NO_LINK, LINK };
}

namespace Memory {
enum Type : bool { LOAD, STORE };
}
// clang-format on

export namespace CPU {

class CPU {
  public:
    CPU(std::shared_ptr<Util::Logger> logger,
        CP0::CP0*                     cp0,
        Memory::Memory*               memory);

    auto registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void;

    auto handleInterrupts() -> void;
    auto runInstruction() -> void;

    // register I/O

    auto readPc() -> uint64_t;

    auto writePc(uint64_t value) -> void;

    template <std::integral T = int32_t>
    auto readGpr(std::size_t index) -> T;

    template <ISA::CPU_REG R, std::integral T = int32_t>
    auto readGpr() -> T;

    template <std::integral T>
    auto writeGpr(std::size_t index, T value) -> void;

    template <ISA::CPU_REG R, std::integral T>
    auto writeGpr(T value) -> void;

    template <std::integral T = int64_t>
    auto readHi() -> T;

    template <std::integral T>
    auto writeHi(T value) -> void;

    template <std::integral T = int64_t>
    auto readLo() -> T;

    template <std::integral T>
    auto writeLo(T value) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    Memory::Memory*               m_memory;
    std::optional<VirtualAddr>    m_delaySlotPc;
    CP0::CP0*                     m_cp0;

    struct { // do not write directly
        std::array<uint64_t, 32> gprs;
        std::array<double, 32>   fprs;
        uint64_t                 pc;
        uint64_t                 hi;
        uint64_t                 lo;
        bool                     llbit;
        float                    fcr0;
        float                    fcr31;
    } m_regs;

    bool                  m_hasBooted;
    std::function<void()> m_bootCallback;
    uint32_t              m_bootAddress;

    // todo genericise to allow jump insts
    template <Branch::Likelihood Likely = Branch::NONE, Branch::Link Link = Branch::NO_LINK, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
    auto executeBranch(uint32_t inst, Function&& func) -> std::optional<uint64_t>;

    template <Branch::Likelihood Likely = Branch::NONE, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, int32_t, int32_t>>
    auto executeBranchAndLink(uint32_t inst, Function&& func) -> std::optional<uint64_t>;

    template <Shift::Len Len, Shift::Dir Dir, Shift::Type Type, Shift::Var Var = Shift::Var::FIXED, Shift::Add Add = Shift::Add::ADD_NONE>
    auto executeShift(uint32_t inst) -> void;

    template <std::integral RegisterType = int32_t, Immediate::Extend I = Immediate::NONE, typename Function>
        requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
    auto executeBivariate(uint32_t inst, Function&& func) -> void;

    template <Immediate::Extend I = Immediate::NONE, std::integral RegisterType = int32_t, typename Function>
        requires std::integral<std::invoke_result_t<Function, RegisterType, RegisterType>>
    auto executeBivariateImmediate(uint32_t inst, Function&& func) -> void;

    template <Memory::Type Type, std::integral T>
    auto executeMemoryOperation(uint32_t inst) -> void;

    template <std::integral T>
    auto executeMultiply(uint32_t inst) -> void;

    template <std::integral T>
    auto executeDivide(uint32_t inst) -> void;
};

CPU::CPU(std::shared_ptr<Util::Logger> logger, CP0::CP0* cp0, Memory::Memory* memory) {
    m_memory    = memory;
    m_logger    = logger;
    m_cp0       = cp0;
    m_regs.gprs = {};
}

auto CPU::registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void {
    m_bootAddress  = callbackBootAddress;
    m_bootCallback = std::move(callback);
}

auto CPU::handleInterrupts() -> void {
    if (m_cp0->hasInterrupt()) {
        auto status = m_cp0->readReg<CP0::Registers::STATUS>();
        status.exl  = 1;
        m_cp0->writeReg(status);
        auto nextPc = m_delaySlotPc.has_value() ? m_regs.pc - 4 : m_regs.pc;
        m_cp0->writeReg<CP0::Registers::EPC>(nextPc);
        m_regs.pc = status.bev ? 0xBFC00000 : 0x80000000;
        IF_LOG_ENABLED(m_logger) {
            m_logger->log<Util::Verbosity::MED>(
                std::tuple{"exceptionStatus", "{:#08x}", std::bit_cast<uint32_t>(status)},
                std::tuple{"exceptionCause", "{:#08x}", std::bit_cast<uint32_t>(m_cp0->readReg<CP0::Registers::CAUSE>())},
                std::tuple{"returnPc", "{:#08x}", nextPc});
        }
    }
}

#include "cpuRegisters.inl"

} // namespace CPU