module;
#include <util/defines.hpp>
export module Emulator:Emulator;

import std;

import CP0;
import CPU;
import Rom;
import RSP;
import RspControl;
import Interfaces;
import ISA;
import Memory;
import Util;

using namespace std::string_view_literals;

export class Emulator {
  public:
    struct Config {
        std::shared_ptr<Util::Logger> logger       = nullptr;
        std::size_t                   memorySize   = 0;
        bool                          dumpRom      = false;
        bool                          dumpPifRom   = false;
        bool                          logAfterBoot = false;
    };

    constexpr Emulator(Config config);
    ~Emulator();

    constexpr auto loadRom(std::filesystem::path romFilePath) -> void;

  private:
    // emulates PIF and IPL3
    constexpr auto emulateInitialBoot() -> void;

    const Config                  m_config;
    std::shared_ptr<Util::Logger> m_logger;

    void*                  m_memory;
    CPU::CPU*              m_cpu;
    CP0::CP0*              m_cp0;
    RSP::RSP*              m_rsp;
    RSP::Control*          m_rspControl;
    std::optional<RomFile> m_rom;
    std::optional<RomFile> m_pifRom;
    Memory::Memory*        m_memoryManager;

    Interfaces::AudioInterface*      m_audioInterface;
    Interfaces::MipsInterface*       m_mipsInterface;
    Interfaces::RdramInterface*      m_rdramInterface;
    Interfaces::RspRegisters*        m_rspRegisters;
    Interfaces::PeripheralInterface* m_peripheralInterface;
    Interfaces::SerialInterface*     m_serialInterface;
    Interfaces::VideoInterface*      m_videoInterface;
};

constexpr auto Emulator::emulateInitialBoot() -> void {
    if (m_pifRom) {
        m_cpu->writePc(static_cast<uint32_t>(0xBFC00000));
        return;
    }

    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Util::Verbosity::MED>("No PIF ROM found, simulating initial boot instead");
    }
    // DMA 1 MiB of ROM code into RSP DMEM
    // these need to be done in 32-bit chunks to ensure correct endianness
    for (auto i = 0uz; i < 0x1000; i += 4) {
        const auto word = m_memoryManager->read<uint32_t>(0xB0000000 + i);
        m_memoryManager->write<uint32_t>(0xA4000000 + i, word);
    }
    m_cpu->writePc(static_cast<uint32_t>(0xA4000040u));

    m_cpu->writeGpr<ISA::CPU_REG::t3>(static_cast<uint32_t>(0xA4000040));
    m_cpu->writeGpr<ISA::CPU_REG::s4>(static_cast<uint32_t>(0x00000001));

    // TODO detect CIC and set this accordingly so we don't fail the checksum
    // CIC_6102 : 0x3F
    // CIC_6105 : 0x91
    m_cpu->writeGpr<ISA::CPU_REG::s6>(static_cast<uint32_t>(0x00000091));
    m_cpu->writeGpr<ISA::CPU_REG::sp>(static_cast<uint32_t>(0xA4001FF0));
    m_cpu->writeGpr<ISA::CPU_REG::ra>(static_cast<uint32_t>(0xA4001000)); // from IPL2 stage

    m_cp0->writeReg<CP0::Registers::RANDOM>(static_cast<uint32_t>(0x0000001F));
    m_cp0->writeReg<CP0::Registers::STATUS>(static_cast<uint32_t>(0x34000000));
    m_cp0->writeReg<CP0::Registers::PRID>(static_cast<uint32_t>(0x00000B00));
    m_cp0->writeReg<CP0::Registers::CONFIG>(static_cast<uint32_t>(0x0006E463));
}

constexpr Emulator::Emulator(Config config) : m_config(config) {
    if (std::filesystem::exists("data/PIF_NTSC_U.bin")) {
        m_pifRom.emplace("data/PIF_NTSC_U.bin");

        if (m_config.dumpPifRom) {
            Util::Logger romDumper{"./pifRom.txt"};
            romDumper.setVerbosity(Util::Verbosity::MAX);
            for (auto i = 0uz; i < m_pifRom->size(); i += 4) {
                const auto word = m_pifRom->read<uint32_t>(i);
                romDumper.logUnstructured("0x{:08x}: {}", i, ISA::Instruction(word));
            }
            romDumper.flush();
            std::println("Dumped PIF ROM to pifRom.txt, exiting...");
            std::terminate();
        }
    }

    m_logger        = config.logger;
    m_memory        = std::malloc(m_config.memorySize);
    m_memoryManager = new Memory::Memory(m_logger, reinterpret_cast<std::byte*>(m_memory));

    m_cp0        = new CP0::CP0(m_logger);
    m_rspControl = new RSP::Control(m_logger);
    m_cpu        = new CPU::CPU(m_logger, m_memoryManager, m_cp0);
    m_rsp        = new RSP::RSP(m_logger, m_memoryManager, m_rspControl);

    m_mipsInterface       = new Interfaces::MipsInterface(m_logger, m_cp0);
    m_rdramInterface      = new Interfaces::RdramInterface(m_logger);
    m_videoInterface      = new Interfaces::VideoInterface(m_logger);
    m_audioInterface      = new Interfaces::AudioInterface(m_logger, m_mipsInterface);
    m_rspRegisters        = new Interfaces::RspRegisters(m_logger, reinterpret_cast<std::byte*>(m_memory), m_mipsInterface, m_rspControl);
    m_peripheralInterface = new Interfaces::PeripheralInterface(m_logger, reinterpret_cast<std::byte*>(m_memory), m_mipsInterface);
    m_serialInterface     = new Interfaces::SerialInterface(m_logger, reinterpret_cast<std::byte*>(m_memory), m_mipsInterface);
    m_serialInterface->loadPifRom(m_pifRom ? &(*(m_pifRom)) : nullptr);

    m_memoryManager->registerAudioInterface(m_audioInterface);
    m_memoryManager->registerMipsInterface(m_mipsInterface);
    m_memoryManager->registerRdramInterface(m_rdramInterface);
    m_memoryManager->registerRspRegisters(m_rspRegisters);
    m_memoryManager->registerPeripheralInterface(m_peripheralInterface);
    m_memoryManager->registerSerialInterface(m_serialInterface);
    m_memoryManager->registerVideoInterface(m_videoInterface);
}

Emulator::~Emulator() {
    std::free(m_memory);
    delete m_cp0;
    delete m_rspControl;
    delete m_cpu;
    delete m_rsp;
    delete m_memoryManager;
    delete m_audioInterface;
    delete m_mipsInterface;
    delete m_rdramInterface;
    delete m_rspRegisters;
    delete m_peripheralInterface;
    delete m_serialInterface;
    delete m_videoInterface;
}

constexpr auto Emulator::loadRom(std::filesystem::path path) -> void {
    m_rom.emplace(path);

    if (m_config.dumpRom) {
        Util::Logger romDumper{"./rom.txt"};
        romDumper.setVerbosity(Util::Verbosity::MAX);
        for (auto i = 0uz; i < m_rom->size(); i += 4) {
            const auto word = m_rom->read<uint32_t>(i);
            romDumper.logUnstructured("0x{:08x}: {}", i, ISA::Instruction(word));
        }
        romDumper.flush();
        std::println("Dumped ROM to rom.txt, exiting...");
        std::terminate();
    }

    m_peripheralInterface->loadRom(&(*(m_rom)));
    emulateInitialBoot();
    if (m_logger) {
        m_logger->log<Util::Verbosity::HIGH>("Loaded ROM: {}", m_rom->readHeader().gameTitle);
    }
    m_cpu->registerBootCallback(m_rom->readHeader().bootAddress, [this]() {
        if (m_config.logAfterBoot) {
            m_logger->enable();
            m_logger->log<Util::Verbosity::HIGH>("Game booted");
        }
    });
    try {
        while (true) {
            m_cpu->handleInterrupts();
            m_cpu->runInstruction();
            m_rsp->runInstruction();
        }
    } catch (const std::runtime_error& e) {
        if (m_logger) {
            m_logger->flush();
        }
        throw;
    }
}
