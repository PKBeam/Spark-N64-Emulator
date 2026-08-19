module;

#include <util/defines.hpp>

export module CPU:VR4300;

import std;
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

export class VR4300 {
  public:
    VR4300(
        std::shared_ptr<Util::Logger> logger,
        Memory::Memory*               memory);

    auto registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void;

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

    template <std::integral T = int32_t>
    auto readCp0Reg(std::size_t index) -> T;

    template <ISA::CP0_REG R, std::integral T = int32_t>
    auto readCp0Reg() -> T;

    template <std::integral T>
    auto writeCp0Reg(std::size_t index, T value) -> void;

    template <ISA::CP0_REG R, std::integral T>
    auto writeCp0Reg(T value) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    Memory::Memory*               m_memory;
    std::optional<VirtualAddr>    m_delaySlotPc;

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

    std::function<void()>    m_bootCallback;
    uint32_t                 m_bootCallbackAddress;
    std::array<uint32_t, 32> m_cp0regs;

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

VR4300::VR4300(std::shared_ptr<Util::Logger> logger, Memory::Memory* memory) {
    m_memory    = memory;
    m_logger    = logger;
    m_regs.gprs = {};
}

auto VR4300::registerBootCallback(uint32_t callbackBootAddress, std::function<void()> callback) -> void {
    m_bootCallbackAddress = callbackBootAddress;
    m_bootCallback        = std::move(callback);
}

struct CP0_STATUS {
    uint32_t ie  : 1;
    uint32_t exl : 1;
    uint32_t erl : 1;
    uint32_t ksu : 1;
    uint32_t ux  : 1;
    uint32_t sx  : 1;
    uint32_t kx  : 1;
    uint32_t im  : 8;
    uint32_t ds  : 9;
    uint32_t re  : 1;
    uint32_t fr  : 1;
    uint32_t rp  : 1;
    uint32_t cu  : 4;
};

#include "vr4300registers.inl"