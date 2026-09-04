module;

#include <util/defines.hpp>

export module RSP:InstructionExecutor;

import std;
import CPU;
import ISA;
import Memory;
import MemoryTypes;
import Util;

import :Registers;

constexpr auto RSP_DMEM_BASE = Util::rangeOf(Memory::PhysSeg::RSP_DMEM).lower;

export namespace Param {
// clang-format off
enum Accumulator  : uint8_t { ACCUM_NONE, ACCUM_ZERO_EXT, ACCUM_SIGN_EXT };
enum AccumOut     : uint8_t { ACCUM_HI_32, ACCUM_LO_32 };
enum CarryIn      : uint8_t { CARRY_IN_NONE, CARRY_IN };
enum OperandSign  : uint8_t { UNSIGNED, SIGNED };
enum ResultClamp  : uint8_t { CLAMP_NONE, CLAMP_UNSIGNED, CLAMP_SIGNED };
enum ProductAccum : uint8_t { ACCUM_SET, ACCUM_ADD };
enum ProductRound : uint8_t { ROUND_NONE, ROUND };
enum MemoryTypeV  : uint8_t { LOADV, STOREV };
enum Mem128bDir   : uint8_t { QUAD, REST };
enum RecipFunc    : uint8_t { RECIP, RECIP_SQRT };
struct Shift { int8_t value; constexpr operator int8_t() const { return value; } };
// clang-format on
} // namespace Param

export namespace RSP {

class InstructionExecutor {
  public:
    InstructionExecutor(std::shared_ptr<Util::Logger> logger,
                        CPU::Registers<Sys::RSP>*     gprs,
                        RSP::Registers*               vprs,
                        Memory::MemoryBus*            memoryBus)
        : m_logger(logger),
          m_cpuExec(logger, gprs, memoryBus),
          m_gprs(gprs),
          m_vprs(vprs),
          m_memoryBus(memoryBus) {}

    auto cpuExec() {
        return &m_cpuExec;
    }

    template <Param::OperandSign VsVtSign, Param::Accumulator Accum = Param::ACCUM_ZERO_EXT, Param::ResultClamp VdClamp = Param::CLAMP_NONE, typename VcoLoFunc = std::nullptr_t, typename VcoHiFunc = std::nullptr_t, Param::CarryIn Carry = Param::CARRY_IN_NONE, typename Function>
        requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>> &&
                 (std::same_as<std::nullptr_t, VcoLoFunc> || std::integral<std::invoke_result_t<VcoLoFunc, uint32_t>>) &&
                 (std::same_as<std::nullptr_t, VcoHiFunc> || std::integral<std::invoke_result_t<VcoHiFunc, uint32_t>>))
    auto executeBivariate(uint32_t inst, Function&& func, VcoLoFunc vcoLoFunc = nullptr, VcoHiFunc vcoHiFunc = nullptr) -> void;

    template <Param::OperandSign VsVtSign, Param::Accumulator Accum = Param::ACCUM_ZERO_EXT, Param::ResultClamp VdClamp = Param::CLAMP_NONE, typename Function>
        requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>)
    auto executeBivariateWithCarryIn(uint32_t inst, Function&& func) -> void {
        executeBivariate<VsVtSign, Accum, VdClamp, std::nullptr_t, std::nullptr_t, Param::CARRY_IN, Function>(inst, std::forward<Function>(func));
    }

    template <Param::ResultClamp VdClamp, Param::OperandSign VsSign, Param::OperandSign VtSign, Param::ProductAccum Accum, Param::AccumOut AccumOut, Param::Shift ShiftValue = Param::Shift(0), Param::ProductRound Round = Param::ROUND_NONE>
        requires(VdClamp != Param::ResultClamp::CLAMP_NONE)
    auto executeMultiply(uint32_t inst) -> void;

    template <Param::MemoryTypeV Type, std::size_t Log2Size>
        requires(Log2Size == 0uz || Log2Size == 1uz || Log2Size == 2uz || Log2Size == 3uz)
    auto executeLoadStore(uint32_t inst) -> void;

    template <Param::MemoryTypeV Type, Param::OperandSign Sign>
    auto executeLoadStorePacked(uint32_t inst) -> void;

    template <Param::MemoryTypeV Type, Param::Mem128bDir Dir>
    auto executeLoadStoreQuad(uint32_t inst) -> void;

    template <Param::Accumulator Accum, typename Function>
        requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>)
    auto executeSingleLane(uint32_t inst, Function&& func) -> void;

    auto executeSingleLaneMove(uint32_t inst) -> void;

    template <typename Function> // vcc <- f(vs, vt, vco low, vco high)
        requires(std::integral<std::invoke_result_t<Function, int16_t, int16_t, bool, bool>>)
    auto executeSelectCompare(uint32_t inst, Function&& func) -> void;

    auto executeSelectMerge(uint32_t inst) -> void;

    auto executeReadAccumulators(uint32_t inst) -> void;

    auto executeReciprocalHigh(uint32_t inst) -> void;

    template <Param::RecipFunc Func>
    auto executeReciprocalLow(uint32_t inst) -> void;

    auto executeSelectClipHigh(uint32_t inst) -> void;

    auto executeSelectClipLow(uint32_t inst) -> void;

    auto executeSelectCrimp(uint32_t inst) -> void;

  private:
    template <std::integral T>
    auto readDMem(uint32_t addr) const -> T;

    template <std::integral T>
    auto writeDMem(uint32_t addr, T value) -> void;

    std::shared_ptr<Util::Logger>      m_logger;
    CPU::InstructionExecutor<Sys::RSP> m_cpuExec;
    CPU::Registers<Sys::RSP>*          m_gprs{};
    RSP::Registers*                    m_vprs{};
    Memory::MemoryBus*                 m_memoryBus{};
};

template <Param::OperandSign VsVtSign, Param::Accumulator Accum, Param::ResultClamp VdClamp, typename VcoLoFunc, typename VcoHiFunc, Param::CarryIn Carry, typename Function>
    requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>> &&
             (std::same_as<std::nullptr_t, VcoLoFunc> || std::integral<std::invoke_result_t<VcoLoFunc, uint32_t>>) &&
             (std::same_as<std::nullptr_t, VcoHiFunc> || std::integral<std::invoke_result_t<VcoHiFunc, uint32_t>>))
auto InstructionExecutor::executeBivariate(uint32_t inst, Function&& func, VcoLoFunc vcoLoFunc, VcoHiFunc vcoHiFunc) -> void {
    using VsVtRegType = std::conditional_t<VsVtSign == Param::SIGNED, int16_t, uint16_t>;
    const auto op     = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vsVpr  = m_vprs->readVpr<VsVtRegType>(op.vs);
    const auto vtVpr  = m_vprs->readVpr<VsVtRegType>(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));

    using VprType = std::conditional_t<Accum == Param::Accumulator::ACCUM_SIGN_EXT, int16_t, uint16_t>;

    constexpr auto hasVcoLoFunc = !std::same_as<std::nullptr_t, VcoLoFunc>;
    constexpr auto hasVcoHiFunc = !std::same_as<std::nullptr_t, VcoHiFunc>;

    static_assert(Carry != Param::CarryIn::CARRY_IN || (!hasVcoLoFunc && !hasVcoHiFunc), "Cannot use carry-in with vco functions");

    auto vco = std::bitset<16>{};
    if constexpr (Carry == Param::CarryIn::CARRY_IN) {
        vco = m_vprs->readVco();
    }
    auto accums = std::array<RSP::Accumulator, 8>{};
    if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
        accums = m_vprs->readAccumulators();
    }

    auto result = VPR<VprType>{};
    for (auto i = 0uz; i < 8; ++i) {
        auto value = func(vsVpr[i], vtVpr[i]);
        if constexpr (Carry == Param::CarryIn::CARRY_IN) {
            value = func(value, vco[i]);
        }
        if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
            accums[i].low = static_cast<uint16_t>(value);
        }
        if constexpr (VdClamp == Param::ResultClamp::CLAMP_SIGNED) {
            value = Util::clamp<int16_t>(value);
        } else if constexpr (VdClamp == Param::ResultClamp::CLAMP_UNSIGNED) {
            value = Util::clamp<uint16_t>(value);
        }
        result[i] = value;
        if constexpr (hasVcoLoFunc) {
            vco[i] = static_cast<bool>(vcoLoFunc(value) & 1);
        }
        if constexpr (hasVcoHiFunc) {
            vco[i + 8] = static_cast<bool>(vcoHiFunc(value) & 1);
        }
    }

    if constexpr (hasVcoLoFunc || hasVcoHiFunc) {
        m_vprs->writeVco(vco);
    } else if constexpr (Carry == Param::CarryIn::CARRY_IN) {
        m_vprs->clearVco();
    }
    if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
        m_vprs->writeAccumulators(accums);
    }
    m_vprs->writeVpr(op.vd, result);
}

template <Param::ResultClamp VdClamp, Param::OperandSign VsSign, Param::OperandSign VtSign, Param::ProductAccum Accum, Param::AccumOut AccumOut, Param::Shift ShiftValue, Param::ProductRound Round>
    requires(VdClamp != Param::ResultClamp::CLAMP_NONE)
auto InstructionExecutor::executeMultiply(uint32_t inst) -> void {
    using VsRegType   = std::conditional_t<VsSign == Param::SIGNED, int16_t, uint16_t>;
    using VtRegType   = std::conditional_t<VtSign == Param::SIGNED, int16_t, uint16_t>;
    using VdClampType = std::conditional_t<VdClamp == Param::CLAMP_SIGNED, int16_t, uint16_t>;

    const auto op    = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vsVpr = m_vprs->readVpr<VsRegType>(op.vs);
    const auto vtVpr = m_vprs->readVpr<VtRegType>(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));

    auto accums = std::array<RSP::Accumulator, 8>{};
    if constexpr (Accum != Param::ProductAccum::ACCUM_SET) {
        accums = m_vprs->readAccumulators();
    }

    auto result = VPR<uint16_t>{};
    for (auto i = 0uz; i < 8; ++i) {
        // to preserve sign, the signed type must be larger than the unsigned
        using VsExtType = std::conditional_t<VsSign == Param::SIGNED, int64_t, uint32_t>;
        using VtExtType = std::conditional_t<VtSign == Param::SIGNED, int64_t, uint32_t>;
        auto product    = static_cast<int64_t>(static_cast<VsExtType>(vsVpr[i]) * static_cast<VtExtType>(vtVpr[i]));

        if constexpr (ShiftValue < 0) {
            product >>= -ShiftValue;
        } else if constexpr (ShiftValue > 0) {
            product <<= ShiftValue;
        }
        if constexpr (Round == Param::ROUND) {
            product += 0x8000;
        }
        if constexpr (Accum == Param::ProductAccum::ACCUM_SET) {
            accums[i] = static_cast<int64_t>(product);
        } else { // ACCUM_ADD
            accums[i] = static_cast<int64_t>(accums[i]) + product;
        }

        const auto accumVal = static_cast<int64_t>(accums[i]);
        auto       out      = accumVal;
        if (accumVal > std::numeric_limits<int32_t>::max()) {
            out = std::numeric_limits<VdClampType>::max();
        } else if (accumVal < std::numeric_limits<int32_t>::min()) {
            out = std::numeric_limits<VdClampType>::min();
        } else {
            if constexpr (AccumOut == Param::ACCUM_HI_32) {
                out >>= 16;
            }
            out &= 0xFFFF;
        }
        result[i] = out;
    }

    m_vprs->writeAccumulators(accums);
    m_vprs->writeVpr(op.vd, result);
}

template <Param::MemoryTypeV Type, std::size_t Log2Size>
    requires(Log2Size == 0uz || Log2Size == 1uz || Log2Size == 2uz || Log2Size == 3uz)
auto InstructionExecutor::executeLoadStore(uint32_t inst) -> void {
    const auto ops        = std::bit_cast<ISA::RSP::TypeVI>(inst);
    const auto signExtImm = static_cast<int32_t>(static_cast<int8_t>(ops.imm << 1) >> 1);
    const auto addr       = static_cast<uint32_t>((signExtImm << Log2Size) + m_gprs->readGpr(ops.rs));
    const auto byteRange  = std::views::iota(ops.vtElem, std::min(16uz, ops.vtElem + (1uz << Log2Size)));

    auto vt = m_vprs->readVpr(ops.vt);
    if constexpr (Type == Param::LOADV) {
        const auto numBytes = std::min(16uz - ops.vtElem, 1uz << Log2Size);
        for (const auto i : std::views::iota(0uz, numBytes)) {
            const auto data    = readDMem<uint8_t>(addr + i);
            const auto byteIdx = ops.vtElem + i;
            vt.setByte(byteIdx, static_cast<uint8_t>(data));
        }
    } else if constexpr (Type == Param::STOREV) {
        constexpr auto numBytes = 1uz << Log2Size;
        for (const auto i : std::views::iota(0uz, numBytes)) {
            const auto byteIdx = (ops.vtElem + i) % 16;
            const auto data    = vt.getByte(byteIdx);
            writeDMem<uint8_t>(addr + i, data);
        }
    }
    if constexpr (Type == Param::LOADV) {
        m_vprs->writeVpr(ops.vt, vt);
    }
}

template <Param::MemoryTypeV Type, Param::OperandSign Sign>
auto InstructionExecutor::executeLoadStorePacked(uint32_t inst) -> void {
    const auto ops        = std::bit_cast<ISA::RSP::TypeVI>(inst);
    const auto signExtImm = static_cast<int32_t>(static_cast<int8_t>(ops.imm << 1) >> 1);
    const auto addr       = static_cast<uint32_t>((signExtImm << 3) + m_gprs->readGpr(ops.rs));

    auto vt = ::RSP::VPR<uint16_t>();
    if constexpr (Type == Param::STOREV) {
        vt = m_vprs->readVpr(ops.vt);
    }
    for (const auto i : std::views::iota(0, 8)) {
        const auto vtIdx    = ops.vtElem + i;
        const auto addrWrap = (addr & ~0b111) + ((addr + i) % 8); // wrap around the current 8-byte block
        auto&      vtElem   = vt[vtIdx % 8];
        if constexpr (Type == Param::LOADV) {
            const auto byte = readDMem<uint8_t>(addrWrap);
            if constexpr (Sign == Param::SIGNED) {
                vtElem = byte << 8;
            } else {
                vtElem = byte << 7;
            }
        } else {
            auto byte = uint8_t();
            if ((Sign == Param::SIGNED && vtIdx < 8) || (Sign == Param::UNSIGNED && vtIdx >= 8)) {
                byte = static_cast<uint8_t>(vtElem >> 8);
            } else {
                byte = static_cast<uint8_t>(vtElem >> 7);
            }
            writeDMem<uint8_t>(addrWrap, byte);
        }
    }
    if constexpr (Type == Param::LOADV) {
        m_vprs->writeVpr(ops.vt, vt);
    }
}

template <Param::MemoryTypeV Type, Param::Mem128bDir Dir>
auto InstructionExecutor::executeLoadStoreQuad(uint32_t inst) -> void {
    const auto ops        = std::bit_cast<ISA::RSP::TypeVI>(inst);
    const auto signExtImm = static_cast<int32_t>(static_cast<int8_t>(ops.imm << 1) >> 1);
    const auto addr       = static_cast<uint32_t>((signExtImm << 4) + m_gprs->readGpr(ops.rs));
    auto       vt         = m_vprs->readVpr(ops.vt);

    const auto addrBytes = [addr] {
        const auto base = static_cast<std::size_t>(addr);
        if constexpr (Dir == Param::QUAD) {
            // addr, addr + 1 ... next aligned address
            return std::views::iota(base, base + 16 - (base % 16));
        } else {
            // addr, addr - 1 ... previous aligned address
            return std::views::iota(base - (base % 16), base + 1) | std::views::reverse;
        }
    }();

    for (auto [i, addrByte] : std::views::enumerate(addrBytes)) {
        const auto vprByte = i + ops.vtElem * 2;
        if constexpr (Type == Param::LOADV) {
            if (vprByte >= 16) {
                break;
            }
            const auto data = readDMem<uint8_t>(addrByte);
            vt.setByte(vprByte, data);
        } else {
            const auto data = vt.getByte(vprByte % 16);
            writeDMem<uint8_t>(addrByte, data);
        }
    }
    if constexpr (Type == Param::LOADV) {
        m_vprs->writeVpr(ops.vt, vt);
    }
}

template <Param::Accumulator Accum, typename Function>
    requires(std::integral<std::invoke_result_t<Function, uint16_t, uint16_t>>)
auto InstructionExecutor::executeSingleLane(uint32_t inst, Function&& func) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVS>(inst);

    const auto vtLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vtElem));
    const auto vdLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vdElem));

    const auto vt = m_vprs->readVpr<uint16_t>(ops.vt);
    auto       vd = m_vprs->readVpr<uint16_t>(ops.vd);

    const auto result = func(vt[vtLane], 0);
    vd[vdLane]        = result;

    if constexpr (Accum != Param::Accumulator::ACCUM_NONE) {
        auto accs = m_vprs->readAccumulators();
        for (auto& acc : accs) {
            acc.low = static_cast<uint16_t>(result);
        }
        m_vprs->writeAccumulators(accs);
    }
    m_vprs->writeVpr(ops.vd, vd);
}

auto InstructionExecutor::executeSingleLaneMove(uint32_t inst) -> void {
    const auto ops    = std::bit_cast<ISA::RSP::TypeVS>(inst);
    const auto vdLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vdElem));
    const auto vt     = m_vprs->readVpr<uint16_t>(ops.vt);

    // write vd
    auto vd    = m_vprs->readVpr<uint16_t>(ops.vd);
    vd[vdLane] = vt[vdLane]; // note: vt intentionally uses the vd lane
    m_vprs->writeVpr(ops.vd, vd);

    // load accumulators
    const auto vtElem  = static_cast<ISA::VEC_ELEM>(ops.vtElem);
    const auto vtBcast = m_vprs->readVpr<uint16_t>(ops.vt, vtElem);
    auto       accs    = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        accs[i].low = vtBcast[i];
    }
    m_vprs->writeAccumulators(accs);
}

template <typename Function>
    requires(std::integral<std::invoke_result_t<Function, int16_t, int16_t, bool, bool>>)
auto InstructionExecutor::executeSelectCompare(uint32_t inst, Function&& func) -> void {
    const auto op  = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vs  = m_vprs->readVpr<int16_t>(op.vs);
    const auto vt  = m_vprs->readVpr<int16_t>(op.vt, static_cast<ISA::VEC_ELEM>(op.vtElem));
    const auto vco = m_vprs->readVco();

    auto vd   = ::RSP::VPR<int16_t>{};
    auto vcc  = std::bitset<16>();
    auto accs = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        vcc[i] = func(vs[i], vt[i], vco[i], vco[i + 8]);

        const auto result = static_cast<int16_t>(vcc[i] ? vs[i] : vt[i]);
        accs[i].low       = static_cast<uint16_t>(result);
        vd[i]             = result;
    }
    m_vprs->writeVcc(vcc);
    m_vprs->writeAccumulators(accs);
    m_vprs->writeVpr(op.vd, vd);
    m_vprs->clearVco();
    m_vprs->clearVce();
}

auto InstructionExecutor::executeSelectMerge(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vcc = m_vprs->readVcc();
    const auto vs  = m_vprs->readVpr<uint16_t>(ops.vs);
    const auto vt  = m_vprs->readVpr<uint16_t>(ops.vt);

    auto vd   = ::RSP::VPR<uint16_t>();
    auto accs = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        vd[i]       = vcc[i] ? vs[i] : vt[i];
        accs[i].low = vd[i];
    }
    m_vprs->clearVco();
    m_vprs->writeVpr(ops.vd, vd);
    m_vprs->writeAccumulators(accs);
}

auto InstructionExecutor::executeReadAccumulators(uint32_t inst) -> void {
    const auto ops    = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto accums = m_vprs->readAccumulators();

    auto vd = ::RSP::VPR<uint16_t>();
    for (auto i : std::views::iota(0, 8)) {
        switch (static_cast<ISA::VEC_ELEM>(ops.vtElem)) {
            case ISA::VEC_ELEM::e0:
                vd[i] = accums[i].low;
                break;
            case ISA::VEC_ELEM::e1:
                vd[i] = accums[i].mid;
                break;
            case ISA::VEC_ELEM::e2:
                vd[i] = accums[i].high;
                break;
            default: throw Util::Error("Invalid element {} for VSAR instruction", static_cast<int>(ops.vtElem));
        }
    };
    m_vprs->writeVpr(ops.vd, vd);
}

auto InstructionExecutor::executeReciprocalHigh(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVS>(inst);

    // read VT into DivIn
    const auto vtLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vtElem));
    const auto vt     = m_vprs->readVpr<uint16_t>(ops.vt);
    m_vprs->writeDivIn(static_cast<uint32_t>(vt[vtLane]) << 16);

    // read VT into Accumulators
    auto accums = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        accums[i].low = vt[i];
    }
    m_vprs->writeAccumulators(accums);

    // write VD with DivOut
    const auto divOut = static_cast<uint16_t>(m_vprs->readDivOut() >> 16);
    const auto vdLane = ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vdElem));
    auto       vd     = m_vprs->readVpr<uint16_t>(ops.vd);
    vd[vdLane]        = divOut;
    m_vprs->writeVpr(ops.vd, vd);
}

template <Param::RecipFunc Func>
auto InstructionExecutor::executeReciprocalLow(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVS>(inst);

    const auto vt = m_vprs->readVpr<uint16_t>(ops.vt)[ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vtElem))];

    auto input = static_cast<uint32_t>(vt);
    input |= m_vprs->readDivIn().value_or(vt >> 15 ? 0xFFFF : 0) << 16;

    // TODO use lookup tables
    const auto signedInput = std::bit_cast<int32_t>(input);
    auto       result      = int32_t();
    if (signedInput == 0) {
        result = 0x7FFFFFFF;
    } else {
        auto resultf = static_cast<double>(signedInput);
        if constexpr (Func == Param::RECIP_SQRT) {
            resultf = std::sqrt(resultf);
        }
        resultf = 1 / resultf;
        if (resultf >= 1.0f) {
            result = 0x7FFFFFFF;
        } else {
            result = resultf * 0x80000000;
        }
    }

    // read VT into Accumulators
    auto accums = m_vprs->readAccumulators();
    for (auto i : std::views::iota(0, 8)) {
        accums[i].low = vt;
    }
    m_vprs->writeAccumulators(accums);

    auto vdVpr                                                        = m_vprs->readVpr<uint16_t>(ops.vd);
    vdVpr[ISA::singleLaneFor(static_cast<ISA::VEC_ELEM>(ops.vdElem))] = static_cast<uint16_t>(result);
    m_vprs->writeVpr(ops.vd, vdVpr);
    m_vprs->writeDivOut(result);
    m_vprs->writeDivIn(std::nullopt);
}

auto InstructionExecutor::executeSelectClipHigh(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vs  = m_vprs->readVpr<int16_t>(ops.vs);
    const auto vt  = m_vprs->readVpr<int16_t>(ops.vt, static_cast<ISA::VEC_ELEM>(ops.vtElem));

    auto vd     = ::RSP::VPR<int16_t>();
    auto accums = m_vprs->readAccumulators();

    m_vprs->clearVcc();
    m_vprs->clearVco();
    m_vprs->clearVce();

    auto vcc = std::bitset<16>{};
    auto vco = std::bitset<16>{};
    auto vce = std::bitset<8>{};

    for (auto i : std::views::iota(0, 8)) {
        const auto sign = (vs[i] & 0x8000) != (vt[i] & 0x8000);

        auto sum = int16_t();
        if (sign) {
            sum           = vs[i] + vt[i];
            accums[i].low = sum <= 0 ? -vt[i] : vs[i];
            vcc[i]        = sum <= 0;
            vcc[i + 8]    = vt[i] < 0;
        } else {
            sum           = vs[i] - vt[i];
            accums[i].low = sum >= 0 ? vt[i] : vs[i];
            vcc[i]        = vt[i] < 0;
            vcc[i + 8]    = sum >= 0;
        }
        vco[i]     = sign;
        vco[i + 8] = sum != 0 && vs[i] != (vt[i] ^ 0xffff);
        vce[i]     = sign ? (sum == -1) : 0;
        vd[i]      = accums[i].low;
    }
    m_vprs->writeAccumulators(accums);
    m_vprs->writeVpr(ops.vd, vd);
    m_vprs->writeVcc(vcc);
    m_vprs->writeVco(vco);
    m_vprs->writeVce(vce);
}

auto InstructionExecutor::executeSelectClipLow(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vs  = m_vprs->readVpr<uint16_t>(ops.vs);
    const auto vt  = m_vprs->readVpr<uint16_t>(ops.vt, static_cast<ISA::VEC_ELEM>(ops.vtElem));
    auto       vcc = m_vprs->readVcc();
    const auto vco = m_vprs->readVco();
    const auto vce = m_vprs->readVce();

    auto vd     = ::RSP::VPR<uint16_t>();
    auto accums = m_vprs->readAccumulators();

    for (auto i : std::views::iota(0, 8)) {
        const auto sign = vco[i];
        auto       acc  = uint16_t();
        if (sign) {
            if (vco[i + 8]) {
                acc = vcc[i] ? -vt[i] : vs[i];
            } else {
                const auto sum   = static_cast<uint32_t>(vs[i]) + static_cast<uint32_t>(vt[i]);
                const auto carry = sum > std::numeric_limits<uint16_t>::max();
                if (vce[i]) {
                    vcc[i] = !sum || !carry;
                } else {
                    vcc[i] = !sum && !carry;
                }
                acc = vcc[i] ? -vt[i] : vs[i];
            }
        } else {
            if (vco[i + 8]) {
                acc = vcc[i + 8] ? vt[i] : vs[i];
            } else {
                vcc[i + 8] = vs[i] >= vt[i];
                acc        = vcc[i + 8] ? vt[i] : vs[i];
            }
        }
        accums[i].low = acc;
        vd[i]         = accums[i].low;
    }
    m_vprs->writeAccumulators(accums);
    m_vprs->writeVpr(ops.vd, vd);
    m_vprs->writeVcc(vcc);
    m_vprs->clearVco();
    m_vprs->clearVce();
}

auto InstructionExecutor::executeSelectCrimp(uint32_t inst) -> void {
    const auto ops = std::bit_cast<ISA::RSP::TypeVR>(inst);
    const auto vs  = m_vprs->readVpr<int16_t>(ops.vs);
    const auto vt  = m_vprs->readVpr<int16_t>(ops.vt, static_cast<ISA::VEC_ELEM>(ops.vtElem));

    auto vd     = ::RSP::VPR<int16_t>();
    auto accums = m_vprs->readAccumulators();

    m_vprs->clearVcc();

    auto vcc = std::bitset<16>{};
    auto vco = std::bitset<16>{};
    auto vce = std::bitset<8>{};

    for (auto i : std::views::iota(0, 8)) {
        const auto sign = (vs[i] ^ vt[i]) < 0;

        auto sum = int16_t();
        if (sign) {
            vcc[i]        = (vs[i] + vt[i] + 1) <= 0;
            vcc[i + 8]    = vt[i] < 0;
            accums[i].low = vcc[i] ? ~vt[i] : vs[i];
        } else {
            vcc[i]        = vt[i] < 0;
            vcc[i + 8]    = vs[i] - vt[i] >= 0;
            accums[i].low = vcc[i + 8] ? vt[i] : vs[i];
        }
        vco[i]     = sign;
        vco[i + 8] = sum != 0;
        vce[i]     = sign ? (sum == -1) : 0;
        vd[i]      = accums[i].low;
    }
    m_vprs->writeAccumulators(accums);
    m_vprs->writeVpr(ops.vd, vd);
    m_vprs->clearVco();
    m_vprs->clearVce();
}

template <std::integral T>
auto InstructionExecutor::readDMem(uint32_t addr) const -> T {
    addr = (addr & 0xFFF) + RSP_DMEM_BASE;

    const auto value = m_memoryBus->readPhysical<T>(addr);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::RSP>(
            std::tuple{"op", "read"},
            std::tuple{"size", sizeof(T)},
            std::tuple{"addr", HEXFMT12, addr - RSP_DMEM_BASE},
            std::tuple{"data", HEXFMT32, static_cast<std::make_unsigned_t<T>>(value)});
    }
    return value;
}

template <std::integral T>
auto InstructionExecutor::writeDMem(uint32_t addr, T value) -> void {
    addr = (addr & 0xFFF) + RSP_DMEM_BASE;
    m_memoryBus->writePhysical<T>(addr, value);
    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sys::RSP>(
            std::tuple{"op", "write"},
            std::tuple{"size", sizeof(T)},
            std::tuple{"addr", HEXFMT12, addr - RSP_DMEM_BASE},
            std::tuple{"data", HEXFMT32, static_cast<std::make_unsigned_t<T>>(value)});
    }
}

} // namespace RSP