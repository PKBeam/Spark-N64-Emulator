module;

#include <util/defines.hpp>

export module RSP:Registers;

import std;
import ISA;
import Util;

export namespace RSP {

struct Registers {
    struct Accumulator {
        uint64_t low  : 16;
        uint64_t mid  : 16;
        uint64_t high : 16;
        uint64_t      : 16;
    };

    Registers(std::shared_ptr<Util::Logger> logger) : m_logger(logger) {}

  private:
    std::shared_ptr<Util::Logger> m_logger;

    std::array<std::array<uint16_t, 8>, 32> m_vprs{};
    std::array<Accumulator, 8>              m_accums{};
    uint16_t                                m_vcc{};
    uint16_t                                m_vco{};
    uint16_t                                m_vce{};
};

} // namespace RSP