module;
#include <util/defines.hpp>
export module Interfaces:Interface;

import std;
import Util;

export namespace Interfaces {

struct WriteableRegister {};

template <typename T>
concept WriteableRegister_c = std::is_base_of_v<T, WriteableRegister>;

class Interface {
  public:
    virtual ~Interface() = default;

    auto sizedRead(uint32_t addr, std::size_t size) -> uint32_t //
        pre(size <= 4);                                         /* 64-bit reads freeze the RCP registers */

    auto sizedWrite(uint32_t addr, std::size_t size, uint32_t data) -> void;

  protected:
    virtual auto read(uint32_t addr) -> uint32_t             = 0;
    virtual auto write(uint32_t addr, uint32_t data) -> void = 0;

    template <typename RegAddrEnum, typename Self>
    auto logOperation(this Self&& self, std::shared_ptr<Util::Logger> logger, std::string_view operationName, uint32_t addr, uint32_t data) -> void;

    template <typename RegAddrStruct, typename Self>
    auto logWarnOnWriteToReadOnlyRegister(this Self&& self, std::shared_ptr<Util::Logger> logger, uint32_t addr) -> void;

    template <typename RegAddrStruct, typename Self>
    auto logWarnOnIgnoredRegister(this Self&& self, std::shared_ptr<Util::Logger> logger, uint32_t addr) -> void;
};

auto Interface::sizedRead(uint32_t addr, std::size_t size) -> uint32_t {
    auto value = read(addr);
    if (size >= 4) {
        return value;
    }
    return (value >> (8 * (addr % 4))) & ((1 << (size * 8)) - 1);
}

auto Interface::sizedWrite(uint32_t addr, std::size_t size, uint32_t data) -> void {
    uint32_t busValue;
    switch (size) {
        case 1: {
            auto byteIndex = addr % 4;
            addr &= 0xFFFFFFFC;
            busValue = data << (8 * (3 - byteIndex));
            break;
        }
        case 2: {
            auto halfIndex = (addr % 4) / 2;
            addr &= 0xFFFFFFFC;
            busValue = data << (16 * (1 - halfIndex));
            break;
        }
        case 4:
            busValue = data;
            break;
        default:
            throw Util::Error("Unsupported register write size: {}", size);
    }
    write(addr, busValue);
}

template <typename RegAddrStruct>
auto getRegisterName(uint32_t addr) -> std::string {
    return Util::enumName(static_cast<RegAddrStruct::Address>(addr)).value_or(std::format("{:#08x}", addr));
}

template <typename RegAddrStruct, typename Self>
auto Interface::logOperation(this Self&& self, std::shared_ptr<Util::Logger> logger, std::string_view operationName, uint32_t addr, uint32_t data) -> void {
    IF_LOG_ENABLED(logger) {
        auto name = getRegisterName<RegAddrStruct>(addr);
        logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", std::meta::display_string_of(^^Self)},
            std::tuple{"op", operationName},
            std::tuple{"reg", "{}", name},
            std::tuple{"data", "0x{:08x}", data});
    }
}

template <typename RegAddrStruct, typename Self>
auto Interface::logWarnOnWriteToReadOnlyRegister(this Self&& self, std::shared_ptr<Util::Logger> logger, uint32_t addr) -> void {
    IF_LOG_ENABLED(logger) {
        auto name = getRegisterName<RegAddrStruct>(addr);
        logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", std::meta::display_string_of(^^Self)},
            std::tuple{"warning", "Attempted to write to read-only register {}", name});
    }
}

template <typename RegAddrStruct, typename Self>
auto Interface::logWarnOnIgnoredRegister(this Self&& self, std::shared_ptr<Util::Logger> logger, uint32_t addr) -> void {
    IF_LOG_ENABLED(logger) {
        auto name = getRegisterName<RegAddrStruct>(addr);
        logger->log<Util::Verbosity::MED>(
            std::tuple{"sys", std::meta::display_string_of(^^Self)},
            std::tuple{"warning", "Ignoring access to register {}", name});
    }
}
} // namespace Interfaces