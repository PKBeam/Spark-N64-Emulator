export module CPU:VR4300;

import std;
import ISA;
import Memory;
import Util;

export class VR4300 {
  public:
    VR4300(
        std::shared_ptr<Memory::Memory> memory,
        std::shared_ptr<Util::Logger>   logger = nullptr);

    auto runInstruction() -> void;

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
    std::shared_ptr<Util::Logger>   m_logger;
    std::shared_ptr<Memory::Memory> m_memory;
    std::optional<VirtualAddr>      m_delaySlotPc;

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

    std::array<uint32_t, 32> m_cp0regs;
};

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

template <std::integral T>
auto VR4300::readGpr(std::size_t index) -> T {
    auto value = static_cast<T>(m_regs.gprs[index]);
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "{}", static_cast<ISA::CPU_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return index == 0 ? 0 : value;
}

template <ISA::CPU_REG R, std::integral T>
auto VR4300::readGpr() -> T {
    return readGpr<T>(static_cast<std::size_t>(R));
}

template <std::integral T>
auto VR4300::writeGpr(std::size_t index, T value) -> void {
    if (index != 0) {
        m_regs.gprs[index] = static_cast<uint32_t>(value); // TODO 64 bit support
        if (m_logger && m_logger->enabled()) {
            m_logger->log<Util::Verbosity::MED>(
                std::tuple{"op", "write"},
                std::tuple{"reg", "{}", static_cast<ISA::CPU_REG>(index)},
                std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
        }
    }
}

template <ISA::CPU_REG R, std::integral T>
auto VR4300::writeGpr(T value) -> void {
    writeGpr<T>(static_cast<std::size_t>(R), value);
}

template <std::integral T>
auto VR4300::readHi() -> T {
    auto value = static_cast<T>(m_regs.hi);
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <std::integral T>
auto VR4300::writeHi(T value) -> void {
    m_regs.hi = Util::signExt32(value);
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "hi"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <std::integral T>
auto VR4300::readLo() -> T {
    auto value = static_cast<T>(m_regs.lo);
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <std::integral T>
auto VR4300::writeLo(T value) -> void {
    m_regs.lo = Util::signExt32(value);
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "lo"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <std::integral T>
auto VR4300::readCp0Reg(std::size_t index) -> T {
    const auto regName = static_cast<ISA::CP0_REG>(index);

    auto value = static_cast<T>(m_cp0regs[index]);
    if (m_logger && m_logger->enabled()) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "CP0 {}", static_cast<ISA::CP0_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

template <ISA::CP0_REG R, std::integral T>
auto VR4300::readCp0Reg() -> T {
    return readCp0Reg(static_cast<std::size_t>(R));
}

template <std::integral T>
auto VR4300::writeCp0Reg(std::size_t index, T value) -> void {
    const auto regName = static_cast<ISA::CP0_REG>(index);
    if (m_logger && regName == ISA::CP0_REG::RANDOM) {
        m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Attempted to write to read-only register CP0_REG::RANDOM!"});
        return;
    }
    if (m_logger && regName == ISA::CP0_REG::STATUS) {
        auto status = std::bit_cast<CP0_STATUS>(static_cast<uint32_t>(value));
        if (status.kx || status.sx || status.ux) {
            m_logger->log<Util::Verbosity::HIGH>(std::tuple{"warning", "Enabled 64-bit mode in CP0_REG::STATUS, which is not fully supported yet"});
        }
    }
    m_cp0regs[index] = Util::signExt32(value);
    if (m_logger && m_logger->enabled()) {
        const auto enumName = Util::enumName(regName);
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "CP0 {}", static_cast<ISA::CP0_REG>(index)},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
}

template <ISA::CP0_REG R, std::integral T>
auto VR4300::writeCp0Reg(T value) -> void {
    writeCp0Reg(static_cast<std::size_t>(R), value);
}

VR4300::VR4300(std::shared_ptr<Memory::Memory> memory, std::shared_ptr<Util::Logger> logger) {
    m_memory    = memory;
    m_logger    = logger;
    m_regs.gprs = {};
}

auto VR4300::runInstruction() -> void {
    [[maybe_unused]] auto instBits = m_memory->read<uint32_t>(m_regs.pc);

    // execute instruction
    auto inst = ISA::Instruction(instBits);

    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"pc", "0x{:08x}", static_cast<uint32_t>(m_regs.pc)},
            std::tuple{"inst", "{}", inst},
            std::tuple{"bits", "0x{:08x}", instBits});
    }

    std::optional<uint64_t> pcJumpValue = std::nullopt;

    inst.opcode.visit([&, this, data = inst.data](auto&& op) {
#if defined(__clang__)
        auto data = inst.data; // silence clang warning
#endif
        using namespace Opcodes;
        using namespace ISA;

        auto checkBranch = [&, this](auto&& comp, bool likely = false) {
            auto ops = std::get<CPU::TypeI>(data);
            if (comp(readGpr(ops.rs), readGpr(ops.rt))) {
                pcJumpValue = (m_regs.pc + 4) + (Util::signExt32<int16_t>(ops.imm) << 2);
            } else if (likely) {
                m_regs.pc += 4;
            }
        };

        if (op == OPCODE::OP_J) {
            auto ops    = std::get<CPU::TypeJ>(data);
            pcJumpValue = ((m_regs.pc + 4) & 0xF0000000) | ops.tgt << 2;

        } else if (op == OPCODE::OP_JAL) {
            auto ops    = std::get<CPU::TypeJ>(data);
            pcJumpValue = ((m_regs.pc + 4) & 0xF0000000) | ops.tgt << 2;
            writeGpr<CPU_REG::ra>(m_regs.pc + 8);

        } else if (op == SPECIAL::OP_JR) {
            auto ops    = std::get<CPU::TypeR>(data);
            pcJumpValue = readGpr(ops.rs);

        } else if (op == SPECIAL::OP_JALR) {
            auto ops    = std::get<CPU::TypeR>(data);
            pcJumpValue = readGpr(ops.rs);
            writeGpr(ops.rd, m_regs.pc + 8);

        } else if (op == OPCODE::OP_BNE) {
            checkBranch([](auto rs, auto rt) { return rs != rt; });

        } else if (op == OPCODE::OP_BNEL) {
            checkBranch([](auto rs, auto rt) { return rs != rt; }, true);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

        } else if (op == REGIMM_rt::OP_BLTZ) {
            checkBranch([](auto rs, auto _) { return rs < 0; });

        } else if (op == REGIMM_rt::OP_BLTZAL) {
            writeGpr<CPU_REG::ra>(m_regs.pc + 8);
            checkBranch([](auto rs, auto _) { return rs < 0; });

        } else if (op == OPCODE::OP_BLEZ) {
            checkBranch([](auto rs, auto _) { return rs <= 0; });

        } else if (op == OPCODE::OP_BLEZL) {
            checkBranch([](auto rs, auto _) { return rs <= 0; }, true);

        } else if (op == REGIMM_rt::OP_BGEZ) {
            checkBranch([](auto rs, auto _) { return rs >= 0; });

        } else if (op == REGIMM_rt::OP_BGEZL) {
            checkBranch([](auto rs, auto _) { return rs >= 0; }, true);

        } else if (op == REGIMM_rt::OP_BGEZAL) {
            auto ops = std::get<CPU::TypeI>(data);

            // detect boot checksum fail
            if (ops.rs == static_cast<uint32_t>(CPU_REG::zero) && static_cast<int16_t>(ops.imm) == -1) {
                if (m_memory->read<uint32_t>(m_regs.pc - 4) == 0) {
                    throw Util::Error("Detected infinite looping BGEZAL->NOP, likely boot checksum fail.");
                }
            }

            writeGpr<CPU_REG::ra>(m_regs.pc + 8);
            checkBranch([](auto rs, auto _) { return rs >= 0; });

#pragma GCC diagnostic pop

        } else if (op == OPCODE::OP_BEQ) {
            checkBranch([](auto rs, auto rt) { return rs == rt; });

        } else if (op == OPCODE::OP_BEQL) {
            checkBranch([](auto rs, auto rt) { return rs == rt; }, true);

        } else if (op == OPCODE::OP_LUI) {
            auto ops = std::get<CPU::TypeI>(data);
            writeGpr(ops.rt, Util::signExt32(ops.imm << 16));

        } else if (op == OPCODE::OP_LB) {
            auto ops   = std::get<CPU::TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
            writeGpr(ops.rt, Util::signExt32(m_memory->read<uint8_t>(vaddr)));

        } else if (op == OPCODE::OP_LBU) {
            auto ops   = std::get<CPU::TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
            writeGpr(ops.rt, m_memory->read<uint8_t>(vaddr));

        } else if (op == OPCODE::OP_LW) {
            auto ops   = std::get<CPU::TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
            writeGpr(ops.rt, Util::signExt32(m_memory->read<uint32_t>(vaddr)));

        } else if (op == OPCODE::OP_LD) {
            throw Util::Error("64-bit not supported yet");
            auto ops   = std::get<CPU::TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
            writeGpr(ops.rt, m_memory->read<uint64_t>(vaddr));

        } else if (op == OPCODE::OP_SB) {
            auto ops   = std::get<CPU::TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
            m_memory->write<uint8_t>(vaddr, readGpr<uint32_t>(ops.rt));

        } else if (op == OPCODE::OP_SW) {
            auto ops   = std::get<CPU::TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
            m_memory->write<uint32_t>(vaddr, readGpr<uint32_t>(ops.rt));

        } else if (op == OPCODE::OP_SWL) {
            auto ops   = std::get<CPU::TypeI>(data);
            auto vaddr = Util::signExt32<int16_t>(ops.imm) + readGpr(ops.rs);
            for (auto byte = 4z; byte > vaddr % 4; --byte) {
                auto thisByte = (readGpr<uint32_t>(ops.rt) >> (8 * byte)) & 0xFF;
                // TODO make this more HW accurate
                m_memory->write<uint8_t>(vaddr + (4 - byte), thisByte);
            }

        } else if (op == SPECIAL::OP_ADD) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr<int32_t>(ops.rs) + readGpr<int32_t>(ops.rt));
            // TODO exception on overflow

        } else if (op == OPCODE::OP_ADDI || op == OPCODE::OP_ADDIU) {
            auto ops = std::get<CPU::TypeI>(data);
            writeGpr(ops.rt, readGpr<int32_t>(ops.rs) + Util::signExt32<int16_t>(ops.imm));
            // TODO exception on overflow for ADDI

        } else if (op == SPECIAL::OP_ADDU) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr(ops.rs) + readGpr(ops.rt));

        } else if (op == SPECIAL::OP_SLT) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr(ops.rs) < readGpr(ops.rt));

        } else if (op == SPECIAL::OP_SLTU) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr<uint32_t>(ops.rs) < readGpr<uint32_t>(ops.rt));

        } else if (op == OPCODE::OP_SLTI) {
            auto ops = std::get<CPU::TypeI>(data);
            writeGpr(ops.rt, readGpr<int32_t>(ops.rs) < Util::signExt32<int16_t>(ops.imm));

        } else if (op == SPECIAL::OP_SUBU) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr(ops.rs) - readGpr(ops.rt));

        } else if (op == SPECIAL::OP_MULTU) {
            auto ops    = std::get<CPU::TypeR>(data);
            auto result = static_cast<uint64_t>(readGpr<uint32_t>(ops.rs)) * static_cast<uint64_t>(readGpr<uint32_t>(ops.rt));
            writeHi(static_cast<int32_t>(result >> 32));
            writeLo(static_cast<int32_t>(result & 0xFFFFFFFF));

        } else if (op == SPECIAL::OP_MFLO) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readLo());

        } else if (op == SPECIAL::OP_MFHI) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readHi());

        } else if (op == SPECIAL::OP_MTLO) {
            auto ops = std::get<CPU::TypeR>(data);
            writeLo(readGpr(ops.rs));

        } else if (op == SPECIAL::OP_MTHI) {
            auto ops = std::get<CPU::TypeR>(data);
            writeHi(readGpr(ops.rs));

        } else if (op == SPECIAL::OP_AND) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr(ops.rs) & readGpr(ops.rt));

        } else if (op == OPCODE::OP_ANDI) {
            auto ops = std::get<CPU::TypeI>(data);
            writeGpr(ops.rt, static_cast<uint16_t>(readGpr(ops.rs)) & ops.imm);

        } else if (op == SPECIAL::OP_OR) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr(ops.rs) | readGpr(ops.rt));

        } else if (op == OPCODE::OP_ORI) {
            auto ops = std::get<CPU::TypeI>(data);
            writeGpr(ops.rt, readGpr(ops.rs) | ops.imm);

        } else if (op == SPECIAL::OP_XOR) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr(ops.rs) ^ readGpr(ops.rt));

        } else if (op == OPCODE::OP_XORI) {
            auto ops = std::get<CPU::TypeI>(data);
            writeGpr(ops.rt, readGpr(ops.rs) ^ ops.imm);

        } else if (op == SPECIAL::OP_SLL) {
            auto ops = std::get<CPU::TypeR>(data);
            if (ops.sa != 0) { // check for NOP
                writeGpr(ops.rd, Util::signExt<int32_t>(readGpr<uint32_t>(ops.rt) << ops.sa));
            }

        } else if (op == SPECIAL::OP_SLLV) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, Util::signExt<int32_t>(readGpr<uint32_t>(ops.rt) << (readGpr(ops.rs) & 0x1F)));

        } else if (op == SPECIAL::OP_SRL) {
            auto ops = std::get<CPU::TypeR>(data);
            if (ops.sa != 0) { // check for NOP
                writeGpr(ops.rd, readGpr<uint32_t>(ops.rt) >> ops.sa);
            }

        } else if (op == SPECIAL::OP_SRLV) {
            auto ops = std::get<CPU::TypeR>(data);
            writeGpr(ops.rd, readGpr<uint32_t>(ops.rt) >> (readGpr(ops.rs) & 0x1F));

        } else if (op == COPz_rs::OP_MTCz) {
            auto ops = std::get<CPU::TypeR>(data);
            writeCp0Reg(ops.rd, readGpr(ops.rt));

        } else if (op == OPCODE::OP_CACHE) {
            if (m_logger) {
                m_logger->log<Util::Verbosity::MED>(
                    std::tuple{"warning", "skipped a CACHE instruction {}", static_cast<uint32_t>(inst)});
            }

        } else {
            if (m_logger) {
                m_logger->flush();
            }
            throw std::runtime_error(
                std::format("Unimplemented instruction @ PC 0x{:08x}: {}", m_regs.pc, inst));
        }
    });

    if (m_delaySlotPc) {
        writePc(*m_delaySlotPc);
        m_delaySlotPc.reset();
    } else {
        m_regs.pc += 4;
    }

    if (pcJumpValue) {
        m_delaySlotPc = pcJumpValue;
    }
}

auto VR4300::readPc() -> uint64_t {
    auto value = m_regs.pc;
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "read"},
            std::tuple{"reg", "PC"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    return value;
}

auto VR4300::writePc(uint64_t value) -> void {
    if (m_logger && m_logger->enabled()) {
        m_logger->log<Util::Verbosity::MED>(
            std::tuple{"op", "write"},
            std::tuple{"reg", "PC"},
            std::tuple{"data", "0x{:08X}", static_cast<uint32_t>(value)});
    }
    m_regs.pc = value;
}
