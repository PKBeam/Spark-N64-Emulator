import std;
import Emulator;
import Util;

using namespace std::string_view_literals;

auto handle_contract_violation(const std::contracts::contract_violation& violation) -> void {
    std::println("Contract violation: {}", violation.comment());
    std::println("{}", std::to_string(std::stacktrace::current()));
    std::terminate();
}

int main(int argc, char* argv[]) {
    // get args
    auto args = std::vector<std::string_view>{};
    for (auto i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    auto emulatorConfig = Emulator::Config{};

    emulatorConfig.memorySize = 0x1FD00000; // maximum size of usable physical memory in N64

    // handle logging
    std::optional<Level> logLevel{};
    std::vector<Sys>     logFilterSys{};
    for (const auto arg : args) {
        if (arg == "--log"sv) {
            logLevel = Level::MED;
        } else if (arg.starts_with("--log=")) {
            auto level = std::string(arg.substr(6));
            try {
                logLevel = static_cast<Level>(std::stoi(level));
            } catch (const std::exception& _) {
                template for (constexpr auto e : Util::staticEnumeratorsOf(^^Level)) {
                    if (std::meta::identifier_of(e) == level) {
                        logLevel = [:e:];
                        break;
                    }
                }
            }
        }
        if (arg == "--log-after-boot"sv) {
            emulatorConfig.logAfterBoot = true;
        }

        if (arg.starts_with("--log-sys=")) {
            auto systems = std::string_view(arg).substr(10);
            for (const auto system : std::views::split(systems, ","sv)) {
                template for (constexpr auto e : Util::staticEnumeratorsOf(^^Sys)) {
                    if (std::meta::identifier_of(e) == std::string_view(system)) {
                        logFilterSys.push_back([:e:]);
                        break;
                    }
                }
            }
        }
        if (arg == "--dump-rom"sv) {
            emulatorConfig.dumpRom = true;
        }
        if (arg == "--dump-pif-rom"sv) {
            emulatorConfig.dumpPifRom = true;
        }
    }

    if (logLevel) {
        emulatorConfig.logger = std::make_shared<Util::Logger>("log.json"sv);
        emulatorConfig.logger->setLevel(*logLevel);
    }
    if (emulatorConfig.logAfterBoot) {
        contract_assert(emulatorConfig.logger != nullptr);
        emulatorConfig.logger->disable();
    }
    if (!logFilterSys.empty()) {
        contract_assert(emulatorConfig.logger != nullptr);
        emulatorConfig.logger->setFilter(logFilterSys);
    }
    auto emu = Emulator(emulatorConfig);
    emu.loadRom("/home/pkbeam/Legend of Zelda, The - Ocarina of Time (USA).z64");

    return 0;
}
