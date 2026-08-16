export module Emulator:Emulator;

import std;

import CPU;
import Rom;
import Interfaces;
import ISA;
import Memory;
import Util;

using namespace std::string_view_literals;

export class Emulator {
  public:
    struct Config {
        std::shared_ptr<Util::Logger> logger     = nullptr;
        std::size_t                   memorySize = 0;
        bool                          dumpRom{};
    };

    constexpr Emulator(Config config);
    ~Emulator();

    constexpr auto loadRom(std::filesystem::path romFilePath) -> void;

  private:
    // emulates PIF and IPL3
    constexpr auto emulateInitialBoot() -> void;

    const Config                  m_config;
    std::shared_ptr<Util::Logger> m_logger;

    void*                           m_memory;
    std::unique_ptr<VR4300>         m_cpu;
    std::optional<RomFile>          m_rom;
    std::shared_ptr<Memory::Memory> m_memoryManager;

    Interfaces::AudioInterface*      m_audioInterface;
    Interfaces::MipsInterface*       m_mipsInterface;
    Interfaces::RdramInterface*      m_rdramInterface;
    Interfaces::RspRegisters*        m_rspRegisters;
    Interfaces::PeripheralInterface* m_peripheralInterface;
    Interfaces::SerialInterface*     m_serialInterface;
};

constexpr auto Emulator::emulateInitialBoot() -> void {
    // DMA 1 MiB of ROM code into memory at the bootAddress
    // these need to be done in 32-bit chunks to ensure correct endianness
    std::optional<Util::Logger> romDumper{};
    if (m_config.dumpRom) {
        romDumper.emplace("./rom_initial_1mib.txt");
        romDumper->setVerbosity(Util::Verbosity::MAX);
    }
    for (auto i = 0uz; i < 0x1000; i += 4) {
        const auto word = m_memoryManager->read<uint32_t>(0xB0000000 + i);
        if (romDumper && romDumper->enabled()) {
            romDumper->logUnstructured("0x{:08x}: {}", i, ISA::Instruction(word));
        }
        m_memoryManager->write<uint32_t>(0xA4000000 + i, word);
    }
    if (romDumper) {
        romDumper->flush();
    }

    m_cpu->writePc(static_cast<uint32_t>(0xA4000040u));

    m_cpu->writeGpr<ISA::CPU_REG::t3>(static_cast<uint64_t>(0xFFFFFFFFA4000040ull));
    m_cpu->writeGpr<ISA::CPU_REG::s4>(static_cast<uint64_t>(0x0000000000000001ull));
    m_cpu->writeGpr<ISA::CPU_REG::s6>(static_cast<uint64_t>(0x000000000000003Full));
    m_cpu->writeGpr<ISA::CPU_REG::sp>(static_cast<uint64_t>(0xFFFFFFFFA4001FF0ull));
    m_cpu->writeGpr<ISA::CPU_REG::ra>(static_cast<uint64_t>(0xFFFFFFFFA4001000ull)); // from IPL2 stage

    m_cpu->writeCp0Reg<ISA::CP0_REG::RANDOM>(static_cast<uint32_t>(0x0000001F));
    m_cpu->writeCp0Reg<ISA::CP0_REG::STATUS>(static_cast<uint32_t>(0x34000000));
    m_cpu->writeCp0Reg<ISA::CP0_REG::PRID>(static_cast<uint32_t>(0x00000B00));
    m_cpu->writeCp0Reg<ISA::CP0_REG::CONFIG>(static_cast<uint32_t>(0x0006E463));
}

constexpr Emulator::Emulator(Config config) : m_config(config) {
    m_logger        = config.logger;
    m_memory        = std::malloc(m_config.memorySize);
    m_memoryManager = std::make_shared<Memory::Memory>(m_logger, reinterpret_cast<std::byte*>(m_memory));

    m_audioInterface      = new Interfaces::AudioInterface(m_logger);
    m_mipsInterface       = new Interfaces::MipsInterface(m_logger);
    m_rdramInterface      = new Interfaces::RdramInterface(m_logger);
    m_rspRegisters        = new Interfaces::RspRegisters(m_logger);
    m_peripheralInterface = new Interfaces::PeripheralInterface(m_logger, reinterpret_cast<std::byte*>(m_memory));
    m_serialInterface     = new Interfaces::SerialInterface(m_logger, reinterpret_cast<std::byte*>(m_memory));

    m_memoryManager->registerAudioInterface(m_audioInterface);
    m_memoryManager->registerMipsInterface(m_mipsInterface);
    m_memoryManager->registerRdramInterface(m_rdramInterface);
    m_memoryManager->registerRspRegisters(m_rspRegisters);
    m_memoryManager->registerPeripheralInterface(m_peripheralInterface);
    m_memoryManager->registerSerialInterface(m_serialInterface);

    m_cpu = std::make_unique<VR4300>(m_memoryManager, m_logger);
}

Emulator::~Emulator() {
    std::free(m_memory);
    delete m_mipsInterface;
    delete m_rdramInterface;
    delete m_rspRegisters;
    delete m_peripheralInterface;
    delete m_serialInterface;
}

constexpr auto Emulator::loadRom(std::filesystem::path path) -> void {
    m_rom.emplace(path);
    m_peripheralInterface->loadRom(&(*(m_rom)));
    emulateInitialBoot();
    if (m_logger) {
        m_logger->log<Util::Verbosity::HIGH>("Loaded ROM: {}", m_rom->readHeader().gameTitle);
    }
    try {
        while (true) {
            m_cpu->runInstruction();
        }
    } catch (const std::runtime_error& e) {
        if (m_logger) {
            m_logger->flush();
        }
        throw;
    }
}
