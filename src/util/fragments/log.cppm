module;
import std;
export module Util:Log;

import :Types;

using namespace std::string_view_literals;

template <typename T>
concept Tuple_c = requires { std::tuple_size<T>::value; };

export namespace Util {

auto toLower(std::string s) -> std::string {
    return s | std::views::transform([](unsigned char c) { return std::tolower(c); }) | std::ranges::to<std::string>();
}

class Logger {
  public:
    enum class Sys {
        NONE,

        // processors
        CPU,
        CP0,
        CP1,
        RDP,
        RSP,

        // memory
        RDRAM,    // memory bus
        PHYS_MEM, // physical memory

        // interfaces
        AI,
        MI,
        PI,
        RDRAM_REG,
        RI,
        RSP_REG,
        RDP_REG,
        SI,
        VI,
    };
    enum class Level : uint8_t {
        MAX       = 255,
        HIGH      = 128,
        MED       = 64,
        LOW       = 32,
        INST_HIGH = 16,
        INST_MED  = 15,
        INST_LOW  = 14,
        NONE      = 0
    };
    enum class Severity {
        INFO,
        WARNING,
        ERROR,
    };

    Logger(std::filesystem::path file) //
        post(m_file != nullptr);

    ~Logger();

    auto enable() -> void;
    auto disable() -> void;

    auto enabled() -> bool;

    auto setLevel(Level level) -> void;
    auto setFilter(std::vector<Sys> sys) -> void;

    template <typename... Args>
    auto print(const char* fmt, Args... obj) -> void;

    template <Logger::Level Level, Logger::Sys Sys = Logger::Sys::NONE, Tuple_c... Args>
    auto log(Args... args) -> void;

    // template <Verbosity v, class T>
    //     requires(!Tuple_c<T>)
    // auto log(T obj) -> void;

    template <Logger::Level Level, Logger::Severity Sev, Logger::Sys Sys = Logger::Sys::NONE, typename... Args>
    auto log(const char* fmt, Args... obj) -> void;

    auto flush() -> void;

  private:
    // template <class T>
    // auto format(T obj) -> std::string;

    Level            m_level = Level::HIGH;
    std::FILE*       m_file{};
    std::vector<Sys> m_sys{};
    bool             m_enabled{};
};

// implementation

Logger::Logger(std::filesystem::path file) {
    m_file    = std::fopen(file.string().c_str(), "w");
    m_enabled = true;
}

Logger::~Logger() {
    flush();
    std::fclose(m_file);
}

auto Logger::enable() -> void {
    m_enabled = true;
}

auto Logger::disable() -> void {
    m_enabled = false;
}

auto Logger::enabled() -> bool {
    return m_enabled;
}

auto Logger::setLevel(Level level) -> void {
    m_level = level;
}

auto Logger::setFilter(std::vector<Sys> sys) -> void {
    m_sys = sys;
}

auto Logger::flush() -> void {
    if (!m_enabled) {
        return;
    }
    std::fflush(m_file);
}

template <typename... Args>
auto Logger::print(const char* fmt, Args... args) -> void {
    if (!m_enabled) {
        return;
    }
    std::println(m_file, std::runtime_format(fmt), args...);
}

template <Logger::Level Level, Logger::Sys Sys, Tuple_c... Args>
auto Logger::log(Args... args) -> void {
    if (!m_enabled || Level < m_level || (!m_sys.empty() && !std::ranges::contains(m_sys, Sys))) {
        return;
    }
    auto str = std::string{"{"};
    if constexpr (Sys != Logger::Sys::NONE) {
        str += std::format("\"sys\": \"{}\"", *Util::enumName(Sys));
    }
    // clang-format off
    (
        (
            [&] {
                std::apply([&](auto const& key, auto const& fmt, auto const&... values) {
                    str += ", "sv;
                    if constexpr (sizeof...(values) == 0) {
                        str += std::format("\"{}\": \"{}\"", key, fmt);
                    } else {
                        str += std::format("\"{}\": \"{}\"",
                                        key,
                                        std::vformat(
                                            std::string_view{fmt},
                                            std::make_format_args(values...)));
                    }
                }, args);
            }()
        ), ...);
    // clang-format on

    str += "}"sv;
    std::println(m_file, "{}", str);
    if constexpr (Level == Logger::Level::MAX) {
        std::println("{}", str);
    }
}

template <Logger::Level Level, Logger::Severity Sev, Logger::Sys Sys, typename... Args>
auto Logger::log(const char* fmt, Args... args) -> void {
    if (!m_enabled || Level < m_level || (!m_sys.empty() && !std::ranges::contains(m_sys, Sys))) {
        return;
    }
    const auto str    = std::format(std::runtime_format(fmt), args...);
    const auto outStr = [&]() {
        if constexpr (Sys != Logger::Sys::NONE) {
            return std::format(
                "{{\"sys\": \"{}\", \"{}\": \"{}\"}}",
                *Util::enumName(Sys),
                Util::toLower(*Util::enumName(Sev)),
                str);
        } else {
            return std::format(
                "{{\"{}\": \"{}\"}}",
                Util::toLower(*Util::enumName(Sev)),
                str);
        }
    }();
    std::println(m_file, "{}", outStr);
    if constexpr (Level == Logger::Level::MAX) {
        std::println("{}", outStr);
    }
}

// template <class T>
// auto Logger::format(T obj) -> std::string {
//     if constexpr (std::formattable<T, char>) {
//         return std::format("\"{}\"", std::format("{}", obj)); // TODO
//     }
//     return std::format("\"{}\"", std::meta::display_string_of(^^T)); // TODO
// }

} // namespace Util

export using Level = Util::Logger::Level;
export using Sev   = Util::Logger::Severity;
export using Sys   = Util::Logger::Sys;
