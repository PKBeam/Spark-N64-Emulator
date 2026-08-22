module;
#include <util/defines.hpp>
export module RspControl:RspControl;

import std;
import Util;

export namespace RSP {

class Control {
  public:
    using SignalSet = std::bitset<8>;

    Control(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {};

    auto setSignal(std::size_t signal, bool value) -> void //
        pre(signal < SignalSet{}.size());

    auto getSignal(std::size_t signal) -> bool //
        pre(signal < SignalSet{}.size());

    template <std::size_t signal>
        requires(signal < SignalSet{}.size())
    auto setSignal(bool value) -> void {
        setSignal(signal, value);
    }

    template <std::size_t signal>
        requires(signal < SignalSet{}.size())
    auto getSignal() -> bool {
        return getSignal(signal);
    }

    auto setPc(uint32_t pc) -> void //
        pre(m_halt);
    auto getPc() -> uint32_t;
    auto incrementPc() -> void;
    auto setSingleStep(bool value) -> void;
    auto getSingleStep() -> bool;
    auto clearBroke() -> void;
    auto getBroke() -> bool;
    auto getHalt() -> bool;
    auto setHalt(bool value) -> void;
    auto setIntBreak(bool value) -> void;
    auto getIntBreak() -> bool;

  private:
    std::shared_ptr<Util::Logger> m_logger;
    uint32_t                      m_pc               = 0;
    SignalSet                     m_signals          = {};
    bool                          m_broke            = false;
    bool                          m_halt             = true;
    bool                          m_singleStep       = false;
    bool                          m_interruptOnBreak = false;
};

auto Control::setPc(uint32_t pc) -> void {
    m_pc = pc;
}

auto Control::getPc() -> uint32_t {
    return m_pc;
}

auto Control::incrementPc() -> void {
    m_pc += 4;
    m_pc &= 0xFFF; // wrap around IMEM
}

auto Control::setSignal(std::size_t signal, bool value) -> void {
    contract_assert(signal < m_signals.size());
    m_signals[signal] = value;
}

auto Control::getSignal(std::size_t signal) -> bool {
    contract_assert(signal < m_signals.size());
    return m_signals[signal];
}

auto Control::setSingleStep(bool value) -> void {
    m_singleStep = value;
}

auto Control::getSingleStep() -> bool {
    return m_singleStep;
}

auto Control::clearBroke() -> void {
    m_broke = false;
}

auto Control::getBroke() -> bool {
    return m_broke;
}

auto Control::getHalt() -> bool {
    return m_halt;
}

auto Control::setHalt(bool value) -> void {
    m_halt = value;
}

auto Control::setIntBreak(bool value) -> void {
    m_interruptOnBreak = value;
}

auto Control::getIntBreak() -> bool {
    return m_interruptOnBreak;
}

} // namespace RSP
