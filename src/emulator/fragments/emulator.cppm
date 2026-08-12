export module Emulator:Emulator;

import std;

import CPU;
import Rom;
import Interfaces;
import ISA;
import Memory;
import Util;

export class Emulator {
  public:
    struct Config {
        std::size_t                    memorySize;
        std::optional<Util::Verbosity> logLevel;
    };

    constexpr Emulator(Config config);

    constexpr auto loadRom(std::filesystem::path romFilePath) -> void;

  private:
    // emulates PIF and IPL3
    constexpr auto emulateInitialBoot() -> void;

    const Config                  m_config;
    std::shared_ptr<Util::Logger> m_logger;

    std::unique_ptr<VR4300>         m_cpu;
    std::shared_ptr<Memory::Memory> m_memory;
};

constexpr auto Emulator::emulateInitialBoot() -> void {
    // DMA 1 MiB of ROM code into memory at the bootAddress
    // these need to be done in 32-bit chunks to ensure correct endianness
    std::optional<Util::Logger> romDumper{};
    if (m_config.logLevel) {
        romDumper.emplace("./rom_initial_1mib.txt");
        romDumper->setVerbosity(*(m_config.logLevel));
    }
    for (auto i = 0uz; i < 0x1000; i += 4) {
        const auto word = m_memory->read<uint32_t>(0xB0000000 + i);
        if (romDumper && romDumper->enabled()) {
            romDumper->logUnstructured("0x{:08x}: {}", i, ISA::Instruction(word));
        }
        m_memory->write<uint32_t>(0xA4000000 + i, word);
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
    using namespace std::string_view_literals;
#ifdef NDEBUG
    if (m_config.logLevel) {
        m_logger = std::make_shared<Util::Logger>("log.json"sv);
        m_logger->setVerbosity(*(m_config.logLevel));
    } else {
        m_logger = nullptr;
    }
#else
    m_logger = std::make_shared<Util::Logger>("log.json"sv);
    m_logger->setVerbosity(Util::Verbosity::MED);
#endif
    m_memory = std::make_shared<Memory::Memory>(config.memorySize, m_logger);
    m_memory->registerMipsInterface(std::move(std::make_unique<Interfaces::MipsInterface>(m_logger)));
    m_memory->registerRdramInterface(std::move(std::make_unique<Interfaces::RdramInterface>(m_logger)));
    m_cpu = std::make_unique<VR4300>(m_memory, m_logger);
}

constexpr auto Emulator::loadRom(std::filesystem::path path) -> void {
    auto rom = RomFile{path};
    m_memory->loadRom(rom);
    emulateInitialBoot();
    if (m_logger) {
        m_logger->log<Util::Verbosity::HIGH>("Loaded ROM: {}", rom.readHeader().gameTitle);
    }
    try {
        while (true) {
            m_cpu->runInstruction();
        }
    } catch (const std::runtime_error& e) {
        if (m_logger) {
            m_logger->flush();
        }
        throw e;
    }
}
