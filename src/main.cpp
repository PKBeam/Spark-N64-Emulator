#define sstr(s) std::define_static_string(s)

import std;
import Emulator;
import Gui;
import Util;

using namespace std::string_view_literals;

auto handle_contract_violation(const std::contracts::contract_violation& violation) -> void {
    std::println("Contract violation: {}", violation.comment());
    std::println("{}", std::to_string(std::stacktrace::current()));
    std::terminate();
}

namespace Args {
struct Info {
    consteval static auto from(std::meta::info memberFunction) -> Info {
        return std::meta::extract<Info>(Util::annotationOf(memberFunction));
    }
    const char* name;
    const char* description;
};

namespace Parsers {

[[= Info(sstr("--log"),
         sstr("Set the log level    (Default: HIGH)"))]] //
    constexpr auto log(Emulator::Config* config, std::string_view LEVEL = "HIGH"sv) -> void {
    auto logLevel = std::optional<Level>{};
    try {
        logLevel = static_cast<Level>(std::stoi(std::string(LEVEL)));
    } catch (const std::exception& _) {
        template for (constexpr auto e : Util::staticEnumeratorsOf(^^Level)) {
            if (std::meta::identifier_of(e) == LEVEL) {
                logLevel = [:e:];
                break;
            }
        }
    }
    if (!logLevel.has_value()) {
        std::println("Available log levels:");
        template for (constexpr auto e : Util::staticEnumeratorsOf(^^Level)) {
            std::println(" - {}", std::meta::identifier_of(e));
        }
        throw Util::Error("Invalid log level: {}", LEVEL);
    }
    config->logger = std::make_shared<Util::Logger>("log.json"sv);
    config->logger->setLevel(*logLevel);
}

[[= Info(sstr("--log-after-boot"),
         sstr("Enable logging after IPL3 boots into the game"))]] //
    constexpr auto logAfterBoot(Emulator::Config* config, std::string_view = ""sv) -> void {
    config->logAfterBoot = true;
    config->logger->disable();
}

[[= Info(sstr("--log-after-rdp-sync"),
         sstr("Enable logging after a certain number of RDP syncs    (Default: 1)"))]] //
    constexpr auto logAfterRdpSync(Emulator::Config* config, std::string_view RDP_SYNC = "1"sv) -> void {
    const auto value = std::stoi(std::string(RDP_SYNC));
    if (value < 1) {
        throw Util::Error("RDP_SYNC must be at least 1", RDP_SYNC);
    }
    config->logAfterRdpSync = value;
    config->logger->disable();
}

[[= Info(sstr("--log-after-pc"),
         sstr("Enable logging after reaching a specific program counter"))]] //
    constexpr auto logAfterPc(Emulator::Config* config, std::string_view PC = ""sv) -> void {
    if (PC.empty()) {
        throw Util::Error("Program counter not specified", PC);
    }
    const auto value = std::stol(std::string(PC), nullptr, 16);
    if (value % 4 != 0) {
        throw Util::Error("Program counter must be aligned to 4 bytes", PC);
    }
    config->logAfterPc = value;
    config->logger->disable();
}

[[= Info(sstr("--log-sys"),
         sstr("Enable per-subsystem logging"))]] //
    constexpr auto logSys(Emulator::Config* config, std::string_view SUBSYSTEMS = ""sv) -> void {
    auto logFilterSys = std::vector<Sys>{};
    for (const auto system : std::views::split(SUBSYSTEMS, ","sv)) {
        if (std::string_view(system) == "ALL_INTERFACES"sv) {
            logFilterSys.push_back(Sys::AI);
            logFilterSys.push_back(Sys::MI);
            logFilterSys.push_back(Sys::PI);
            logFilterSys.push_back(Sys::RDRAM_REG);
            logFilterSys.push_back(Sys::RI);
            logFilterSys.push_back(Sys::RSP_REG);
            logFilterSys.push_back(Sys::RDP_REG);
            logFilterSys.push_back(Sys::SI);
            logFilterSys.push_back(Sys::VI);
        }
        template for (constexpr auto e : Util::staticEnumeratorsOf(^^Sys)) {
            if (std::meta::identifier_of(e) == std::string_view(system)) {
                logFilterSys.push_back([:e:]);
                break;
            }
        }
    }
    if (logFilterSys.empty()) {
        std::println("Available subsystems:");
        template for (constexpr auto e : Util::staticEnumeratorsOf(^^Sys)) {
            std::println(" - {}", std::meta::identifier_of(e));
        }
        throw Util::Error("Invalid subsystem: {}", SUBSYSTEMS);
    }
    config->logger->setFilter(logFilterSys);
}
[[= Info(sstr("--dump-rom"),
         sstr("Dump the ROM contents to a file"))]] //
    constexpr auto dumpRom(Emulator::Config* config, std::string_view = ""sv) -> void {
    config->dumpRom = true;
}
[[= Info(sstr("--dump-pif-rom"),
         sstr("Dump the PIF ROM contents to a file"))]] //
    constexpr auto dumpPifRom(Emulator::Config* config, std::string_view = ""sv) -> void {
    config->dumpPifRom = true;
}
[[= Info(sstr("--num-rdp-syncs"),
         sstr("Terminate after a certain number of RDP syncs    (Default: 1)"))]] //
    constexpr auto numRdpSyncs(Emulator::Config* config, std::string_view RDP_SYNCS = "1"sv) -> void {
    const auto value = std::stoi(std::string(RDP_SYNCS));
    if (value < 1) {
        throw Util::Error("RDP_SYNC must be at least 1", RDP_SYNCS);
    }
    config->terminateAfterRdpSyncs = value;
}

[[= Info(sstr("--help"),
         sstr("Print help and exit"))]] //
    constexpr auto help(Emulator::Config* = nullptr, std::string_view = ""sv) -> void {
    constexpr static auto members = Util::staticMembersOf(^^Args::Parsers);
    static_assert(members.size() > 1, "Args::Parsers::help() must be declared last");
    std::println("Options:\n");
    template for (constexpr auto fn : members) {
        constexpr auto fnArg = Args::Info::from(fn);
        constexpr auto param = std::meta::parameters_of(fn)[1];
        if constexpr (std::meta::has_identifier(param)) {
            constexpr auto paramName = std::meta::has_identifier(param) ? std::meta::identifier_of(param) : "";
            std::println("{}={}\n    {}\n", fnArg.name, paramName, fnArg.description);
            continue;
        }
        std::println("{}\n    {}\n", fnArg.name, fnArg.description);
        continue;
    }
    std::exit(0);
}

} // namespace Parsers
auto parse(const std::vector<std::string_view>& args) -> Emulator::Config {
    auto                  config  = Emulator::Config{};
    constexpr static auto members = Util::staticMembersOf(^^Args::Parsers);
    for (const auto arg : args) {
        template for (constexpr auto fn : members) {
            constexpr Args::Info fnArg     = Args::Info::from(fn);
            constexpr auto       fnArgName = std::string_view(fnArg.name);
            if (arg.starts_with(fnArgName)) {
                if (arg == fnArgName) {
                    [:fn:](&config);
                } else if (arg.size() > fnArgName.size() && arg[fnArgName.size()] == '=') {
                    [:fn:](&config, arg.substr(fnArgName.size() + 1));
                }
            }
        }
    }
    return config;
}
} // namespace Args

constinit auto g_shouldTerminate = std::atomic<bool>{false};
auto           signalHandler(int signal) -> void {
    g_shouldTerminate = true;
}

int main(int argc, char* argv[]) {
    // get args
    auto args = std::vector<std::string_view>{};
    for (auto i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    auto config       = Args::parse(args);
    config.memorySize = 0x1FD00000; // maximum size of usable physical memory in N64

    auto app    = GUI::Application(argc, argv, g_shouldTerminate);
    auto window = GUI::Window(app.getVulkanInstance());
    window.show();

    auto emulator       = Emulator(config);
    auto emulatorThread = std::jthread([&emulator]() {
        emulator.loadRom("/home/pkbeam/Legend of Zelda, The - Ocarina of Time (USA).z64");
        while (!g_shouldTerminate) {
            emulator.runCycle();
        }
    });

    auto timer = Util::Timer<60>([&]() {
        emulator.processNextFrame();
    });
    std::signal(Util::SigInt, signalHandler);
    std::signal(Util::SigTerm, signalHandler);

    auto code = app.run();
    return code;
}
