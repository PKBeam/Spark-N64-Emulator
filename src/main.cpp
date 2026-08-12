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

    // handle logging
    std::optional<Util::Verbosity> logLevel{};
    for (const auto arg : args) {
        if (arg == "--log"sv) {
            logLevel = Util::Verbosity::MED;
        } else if (arg.starts_with("--log=")) {
            auto level = std::string{arg.substr(6)};
            logLevel   = static_cast<Util::Verbosity>(std::stoi(level));
        }
    }

    auto emu = Emulator({
        .memorySize = 0x1FD00000,
        .logLevel   = logLevel,
    });
    emu.loadRom("/home/pkbeam/ZOOTDEC.z64");

    return 0;
}
