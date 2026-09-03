module;
#include <util/defines.hpp>
export module CP1:CP1;

import std;
import ISA;
import Util;

import :InstructionExecutor;
import :Registers;

export namespace CP1 {

class CP1 {
  public:
    CP1(std::shared_ptr<Util::Logger> logger, Memory::MemoryBus* memoryBus) : m_logger(logger), m_regs(logger), m_exec(logger, &m_regs, memoryBus) {}

    constexpr auto setFgrMode(::CP1::Registers::Mode mode) -> void {
        m_regs.setMode(mode);
    }

    constexpr auto getExec() -> ::CP1::InstructionExecutor* {
        return &m_exec;
    }

    constexpr auto getRegs() -> ::CP1::Registers* {
        return &m_regs;
    }

  private:
    std::shared_ptr<Util::Logger> m_logger;
    ::CP1::Registers              m_regs;
    ::CP1::InstructionExecutor    m_exec;
};

} // namespace CP1
