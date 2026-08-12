export module Interfaces:MmioRegisters;

import std;
import Util;

export namespace Interfaces {

struct WriteableRegister {};

template <typename T>
concept WriteableRegister_c = std::is_base_of_v<T, WriteableRegister>;

class MmioRegisters {
  public:
    virtual ~MmioRegisters() = default;

    auto sizedRead(uint32_t addr, std::size_t size) -> uint32_t //
        pre(size <= 4);                                         /* 64-bit reads freeze the RCP registers */

    auto sizedWrite(uint32_t addr, std::size_t size, uint32_t data) -> void;

  private:
    virtual auto read(uint32_t addr) -> uint32_t = 0;

    virtual auto write(uint32_t addr, uint32_t data) -> void = 0;
};

auto MmioRegisters::sizedRead(uint32_t addr, std::size_t size) -> uint32_t {
    auto value = read(addr);
    return (value >> (8 * (addr % 4))) & ((1 << (size * 8)) - 1);
}

auto MmioRegisters::sizedWrite(uint32_t addr, std::size_t size, uint32_t data) -> void {
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
            throw std::runtime_error(std::format(
                "Unsupported register write size: {}", size));
    }
    write(addr, busValue);
}
} // namespace Interfaces