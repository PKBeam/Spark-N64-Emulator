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
enum MemoryTypeFloat : bool { LOAD_F,
                              STORE_F };
enum ComparisonOrder : bool { ORDERED,
                              UNORDERED };
enum ComparisonSignal : bool { NO_SIGNAL,
                               SIGNAL };
// clang-format on
} // namespace Param

export namespace CP1 {

namespace ExceptionFunc { // TODO inexact/overflow/underflow exceptions, denorm/QNaN
constexpr auto ADD = [](std::floating_point auto a, std::floating_point auto b) -> ISA::CP1_EXCEPTION {
    if (Util::isSNaN(a) || Util::isSNaN(b)) return ISA::CP1_EXCEPTION::INVALID_OP;
    if (std::isinf(a) && std::isinf(b)) return ISA::CP1_EXCEPTION::INVALID_OP;
    return ISA::CP1_EXCEPTION::NONE; };

constexpr auto MUL = [](std::floating_point auto a, std::floating_point auto b) -> ISA::CP1_EXCEPTION {
    if (Util::isSNaN(a) || Util::isSNaN(b)) return ISA::CP1_EXCEPTION::INVALID_OP;
    if (a == 0 && std::isinf(b)) return ISA::CP1_EXCEPTION::INVALID_OP;
    return ISA::CP1_EXCEPTION::NONE; };

constexpr auto DIV = [](std::floating_point auto a, std::floating_point auto b) -> ISA::CP1_EXCEPTION {
    if (Util::isSNaN(a) || Util::isSNaN(b)) return ISA::CP1_EXCEPTION::INVALID_OP;
    if ((a == 0 && b == 0) || (std::isinf(a) && std::isinf(b))) return ISA::CP1_EXCEPTION::INVALID_OP;
    if (b == 0) return ISA::CP1_EXCEPTION::DIVIDE_BY_ZERO;
    return ISA::CP1_EXCEPTION::NONE;
};
constexpr auto SQRT = [](std::floating_point auto a, std::floating_point auto b) -> ISA::CP1_EXCEPTION {
    if (Util::isSNaN(a) || Util::isSNaN(b)) return ISA::CP1_EXCEPTION::INVALID_OP;
    if (a < 0) return ISA::CP1_EXCEPTION::INVALID_OP;
    return ISA::CP1_EXCEPTION::NONE;
};
constexpr auto ABS = [](std::floating_point auto a, std::floating_point auto b) -> ISA::CP1_EXCEPTION {
    if (Util::isSNaN(a) || Util::isSNaN(b)) return ISA::CP1_EXCEPTION::INVALID_OP;
    return ISA::CP1_EXCEPTION::NONE;
};
constexpr auto NEG = [](std::floating_point auto a, std::floating_point auto b) -> ISA::CP1_EXCEPTION {
    if (Util::isSNaN(a) || Util::isSNaN(b)) return ISA::CP1_EXCEPTION::INVALID_OP;
    return ISA::CP1_EXCEPTION::NONE;
};

static_assert(std::same_as<ISA::CP1_EXCEPTION, std::invoke_result_t<decltype(NEG), float, float>>);

} // namespace ExceptionFunc

class InstructionExecutor {
  public:
    InstructionExecutor(std::shared_ptr<Util::Logger> logger, CP1::Registers* fprs, Memory::Memory* memory)
        : m_logger(logger), m_fprs(fprs), m_memory(memory) {}

    template <typename To>
        requires(FloatType_c<To>)
    auto executeConvert(uint32_t inst, Util::FP_ROUND_MODE roundMode) -> void;

    template <typename To>
        requires(FloatType_c<To>)
    auto executeConvert(uint32_t inst) -> void {
        executeConvert<To>(inst, m_fprs->getRoundingMode());
    }

    template <Param::ComparisonOrder Order = Param::ORDERED, Param::ComparisonSignal Signal = Param::NO_SIGNAL, typename Function>
        requires std::same_as<bool, std::invoke_result_t<Function, float, float>>
    auto executeCompare(uint32_t inst, Function&& func) -> void;

    template <typename Function, typename ExceptionFunc = std::nullptr_t>
        requires(std::floating_point<std::invoke_result_t<Function, float, float>> &&
                 (std::same_as<std::nullptr_t, ExceptionFunc> || std::same_as<ISA::CP1_EXCEPTION, std::invoke_result_t<ExceptionFunc, float, float>>))
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
auto InstructionExecutor::executeConvert(uint32_t inst, Util::FP_ROUND_MODE roundMode) -> void {
    Util::withFpRoundMode(roundMode, [this, inst]() {
        const auto ops  = std::bit_cast<ISA::FPU::TypeR>(inst);
        const auto fmt  = static_cast<ISA::CP1_FORMAT>(ops.fmt);
        const auto from = m_fprs->readFpr(ops.fs, fmt);
        from.visit([this, &ops](auto value) {
            m_fprs->writeFpr<To>(ops.fd, static_cast<To>(value));
        });
    });
}

template <Param::ComparisonOrder Order, Param::ComparisonSignal Signal, typename Function>
    requires std::same_as<bool, std::invoke_result_t<Function, float, float>>
auto InstructionExecutor::executeCompare(uint32_t inst, Function&& func) -> void {
    const auto ops = std::bit_cast<ISA::FPU::TypeR>(inst);
    const auto fmt = static_cast<ISA::CP1_FORMAT>(ops.fmt);

    ISA::getFormatType(fmt).visit([this, &func, &ops](auto T) {
        const auto fs = m_fprs->readFpr<decltype(T)>(ops.fs);
        const auto ft = m_fprs->readFpr<decltype(T)>(ops.ft);

        auto result = bool();
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
             (std::same_as<std::nullptr_t, ExceptionFunc> || std::same_as<ISA::CP1_EXCEPTION, std::invoke_result_t<ExceptionFunc, float, float>>))
auto InstructionExecutor::executeBivariate(uint32_t inst, Function&& func, ExceptionFunc exceptFunc) -> void {
    const auto ops = std::bit_cast<ISA::FPU::TypeR>(inst);
    const auto fmt = static_cast<ISA::CP1_FORMAT>(ops.fmt);

    ISA::getFormatType(fmt).visit([this, &func, &exceptFunc, &ops](auto T) {
        const auto fs = m_fprs->readFpr<decltype(T)>(ops.fs);
        const auto ft = m_fprs->readFpr<decltype(T)>(ops.ft);

        if constexpr (!std::is_same_v<ExceptionFunc, std::nullptr_t> && std::is_floating_point_v<decltype(T)>) {
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

    auto data = T{};
    if constexpr (Type == Param::MemoryTypeFloat::LOAD_F) {
        data = m_memory->read<T>(addr);
        m_fprs->writeFgr<T>(ops.ft, data);
    } else {
        data = m_fprs->readFgr<T>(ops.ft);
        m_memory->write<T>(addr, data);
    }
}

} // namespace CP1