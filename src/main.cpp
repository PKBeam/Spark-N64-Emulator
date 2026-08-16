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
    std::optional<Util::Verbosity> logLevel{};
    for (const auto arg : args) {
        if (arg == "--log"sv) {
            logLevel = Util::Verbosity::MED;
        } else if (arg.starts_with("--log=")) {
            auto level = std::string{arg.substr(6)};
            try {
                logLevel = static_cast<Util::Verbosity>(std::stoi(level));
            } catch (const std::exception& _) {
                template for (constexpr auto e : Util::staticEnumeratorsOf(^^Util::Verbosity)) {
                    if (std::meta::identifier_of(e) == level) {
                        logLevel = [:e:];
                        break;
                    }
                }
            }
        } else if (arg == "--dump-rom"sv) {
            emulatorConfig.dumpRom = true;
        }
    }

    if (logLevel) {
        emulatorConfig.logger = std::make_shared<Util::Logger>("log.json"sv);
        emulatorConfig.logger->setVerbosity(*logLevel);
    }

    auto emu = Emulator(emulatorConfig);
    emu.loadRom("/home/pkbeam/ZOOTDEC.z64");

    return 0;
}
