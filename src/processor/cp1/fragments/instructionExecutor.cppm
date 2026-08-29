module;
#include <util/defines.hpp>
export module CP1:InstructionExecutor;

import std;
import ISA;
import Memory;
import Util;

import :Registers;

export namespace Param {
// clang-format off
enum MemoryTypeFloat  : bool { LOAD_F, STORE_F };
enum ComparisonOrder  : bool { ORDERED, UNORDERED };
enum ComparisonSignal : bool { NO_SIGNAL, SIGNAL };
// clang-format on
} // namespace Param

export namespace CP1 {

namespace ExceptionFunc {
constexpr auto DivisionByZero = [](auto fs, auto ft) -> ISA::CP1_EXCEPTION {
    if (ft == 0) {
        return ISA::CP1_EXCEPTION::DIVIDE_BY_ZERO;
    }
    return ISA::CP1_EXCEPTION::NONE;
};
} // namespace ExceptionFunc

class InstructionExecutor {
  public:
    InstructionExecutor(std::shared_ptr<Util::Logger> logger, CP1::Registers* fprs, Memory::Memory* memory)
        : m_logger(logger), m_fprs(fprs), m_memory(memory) {}

    template <typename To>
        requires(FloatType_c<To>)
    auto executeConvert(uint32_t inst) -> void;

    template <Param::ComparisonOrder Order = Param::ORDERED, Param::ComparisonSignal Signal = Param::NO_SIGNAL, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, float, float>>
    auto executeCompare(uint32_t inst, Function&& func) -> void;

    template <typename Function, typename ExceptionFunc = std::nullptr_t>
        requires(std::floating_point<std::invoke_result_t<Function, float, float>> &&
                 (std::same_as<std::nullptr_t, ExceptionFunc> || std::same_as<ISA::CP1_EXCEPTION, std::invoke_result_t<Function, float, float>>))
    auto executeBivariate(uint32_t inst, Function&& func, ExceptionFunc exceptFunc = nullptr) -> void;

    template <Param::MemoryTypeFloat Type, std::integral T, std::integral U>
    auto executeMemoryOperation(uint32_t inst, U gprValue) -> void;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    CP1::Registers*               m_fprs{};
    Memory::Memory*               m_memory{};
};

template <typename To>
    requires(FloatType_c<To>)
auto InstructionExecutor::executeConvert(uint32_t inst) -> void { // TODO rounding
    const auto ops  = std::bit_cast<ISA::FPU::TypeR>(inst);
    const auto fmt  = static_cast<ISA::CP1_FORMAT>(ops.fmt);
    const auto from = m_fprs->readFpr(ops.fs, fmt);

    const auto originalRounding = fegetround();
    fesetround(m_fprs->getRoundingMode());
    from.visit([this, &ops](auto value) {
        m_fprs->writeFpr<To>(ops.fd, static_cast<To>(value));
    });
    fesetround(originalRounding);
}

template <Param::ComparisonOrder Order, Param::ComparisonSignal Signal, typename Function>
    requires std::same_as<bool, std::invoke_result_t<Function, float, float>>
auto InstructionExecutor::executeCompare(uint32_t inst, Function&& func) -> void {
    const auto ops = std::bit_cast<ISA::FPU::TypeR>(inst);
    const auto fmt = static_cast<ISA::CP1_FORMAT>(ops.fmt);

    ISA::getFormatType(fmt).visit([this, &func, &ops](auto T) {
        const auto fs = m_fprs->readFpr<decltype(T)>(ops.fs);
        const auto ft = m_fprs->readFpr<decltype(T)>(ops.ft);

        bool result{};
        if (std::isnan(fs) || std::isnan(ft)) {
            if constexpr (Signal == Param::SIGNAL) {
                // TODO signal exception
                IF_LOG_ENABLED(m_logger) {
                    m_logger->log<Level::HIGH, Sev::WARNING, Sys::CP1>("Ignored signalled exception during comparison");
                }
                return;
            } else {
                result = (Order == Param::ComparisonOrder::UNORDERED);
            }
        } else {
            result = func(fs, ft);
        }

        auto status = WITH_LOG_DISABLED(m_logger, m_fprs->readStatus());
        status.c    = result;
        m_fprs->writeStatus(status);
    });
}

template <typename Function, typename ExceptionFunc>
    requires(std::floating_point<std::invoke_result_t<Function, float, float>> &&
             (std::same_as<std::nullptr_t, ExceptionFunc> || std::same_as<ISA::CP1_EXCEPTION, std::invoke_result_t<Function, float, float>>))
auto InstructionExecutor::executeBivariate(uint32_t inst, Function&& func, ExceptionFunc exceptFunc) -> void {
    const auto ops = std::bit_cast<ISA::FPU::TypeR>(inst);
    const auto fmt = static_cast<ISA::CP1_FORMAT>(ops.fmt);

    ISA::getFormatType(fmt).visit([this, &func, &exceptFunc, &ops](auto T) {
        const auto fs = m_fprs->readFpr<decltype(T)>(ops.fs);
        const auto ft = m_fprs->readFpr<decltype(T)>(ops.ft);

        if constexpr (!std::is_same_v<ExceptionFunc, std::nullptr_t>) {
            const auto exception = exceptFunc(fs, ft);
            if (exception != ISA::CP1_EXCEPTION::NONE) {
                throw Util::Error("FPU exception {} occurred during instruction execution", static_cast<int>(exception));
            }
        }
        const auto result = func(fs, ft);
        m_fprs->writeFpr(ops.fd, result);
    });
}

template <Param::MemoryTypeFloat Type, std::integral T, std::integral U>
auto InstructionExecutor::executeMemoryOperation(uint32_t inst, U gprValue) -> void {
    const auto ops  = std::bit_cast<ISA::FPU::TypeI>(inst);
    const auto addr = Util::signExt32<int16_t>(ops.imm) + gprValue;

    T data{};
    if constexpr (Type == Param::MemoryTypeFloat::LOAD_F) {
        data = m_memory->read<T>(addr);
        m_fprs->writeFgr<T>(ops.ft, data);
    } else {
        data = m_fprs->readFgr<T>(ops.ft);
        m_memory->write<T>(addr, data);
    }
}

} // namespace CP1