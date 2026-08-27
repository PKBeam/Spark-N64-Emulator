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
import InterfaceTypes;
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

    void*                  m_memory{};
    CPU::CPU*              m_cpu{};
    CP0::CP0*              m_cp0{};
    RSP::RSP*              m_rsp{};
    RSP::Control*          m_rspControl{};
    std::optional<RomFile> m_rom;
    std::optional<RomFile> m_pifRom;
    Memory::Memory*        m_memoryManager{};

    Interfaces::AudioInterface*      m_audioInterface{};
    Interfaces::MipsInterface*       m_mipsInterface{};
    Interfaces::RdramInterface*      m_rdramInterface{};
    Interfaces::RspRegisters*        m_rspRegisters{};
    Interfaces::PeripheralInterface* m_peripheralInterface{};
    Interfaces::SerialInterface*     m_serialInterface{};
    Interfaces::VideoInterface*      m_videoInterface{};
};

constexpr auto Emulator::emulateInitialBoot() -> void {
    if (m_pifRom) {
        return;
    }

    IF_LOG_ENABLED(m_logger) {
        m_logger->log<Level::HIGH, Sev::WARNING>("No PIF ROM found, simulating initial boot instead");
    }
    m_cpu->emulateInitialBoot();
}

constexpr Emulator::Emulator(Config config) : m_config(config) {
    if (std::filesystem::exists("data/PIF_NTSC_U.bin")) {
        m_pifRom.emplace("data/PIF_NTSC_U.bin");
        if (m_config.dumpPifRom) {
            m_pifRom->dump("./pifRom.txt");
            std::println("Dumped PIF ROM to pifRom.txt, exiting...");
            std::terminate();
        }
    }

    m_logger        = config.logger;
    m_memory        = std::malloc(m_config.memorySize);
    m_memoryManager = new Memory::Memory(m_logger, reinterpret_cast<std::byte*>(m_memory));

    m_cp0           = new CP0::CP0(m_logger);
    m_mipsInterface = new Interfaces::MipsInterface(m_logger, m_cp0);

    m_cpu = new CPU::CPU(m_logger, m_memoryManager, m_cp0);

    m_rspControl = new RSP::Control(m_logger, reinterpret_cast<std::byte*>(m_memory));
    m_rsp        = new RSP::RSP(m_logger, m_memoryManager, m_rspControl, m_mipsInterface);

    m_rdramInterface      = new Interfaces::RdramInterface(m_logger);
    m_videoInterface      = new Interfaces::VideoInterface(m_logger, m_mipsInterface);
    m_audioInterface      = new Interfaces::AudioInterface(m_logger, m_mipsInterface);
    m_rspRegisters        = new Interfaces::RspRegisters(m_logger, m_mipsInterface, m_rspControl);
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
        m_rom->dump("./rom.txt");
        std::println("Dumped ROM to rom.txt, exiting...");
        std::terminate();
    }

    m_peripheralInterface->loadRom(&(*(m_rom)));
    emulateInitialBoot();
    if (m_logger) {
        m_logger->log<Level::HIGH, Sev::INFO>("Loaded ROM: '{}'", m_rom->readHeader().gameTitle);
    }
    m_cpu->registerBootCallback(m_rom->readHeader().bootAddress, [this]() {
        if (m_config.logAfterBoot) {
            m_logger->enable();
            m_logger->log<Level::HIGH, Sev::INFO>("Game booted");
        }
    });
    try {
        while (true) {
            // if (m_videoInterface->hasTimerFired()) {
            //     m_mipsInterface->setInterrupt<^^Interfaces::MI_INTERRUPT::vi>(true);
            //     m_videoInterface->clearTimerFired();
            // }
            m_cpu->checkInterrupts();
            m_cpu->runInstruction();
            try {
                m_rsp->runInstruction();
            } catch (const Util::Error& e) {
                m_rsp->dumpIMem("rsp_imem.txt");
                throw;
            }
        }
    } catch (const Util::Error& e) {
        if (m_logger) {
            m_logger->flush();
        }
        throw;
    }
}