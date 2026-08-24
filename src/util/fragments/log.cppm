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
        RSP,

        // memory
        RDRAM,

        // interfaces
        AI,
        MI,
        PI,
        RDRAM_REG,
        RI,
        RSP_REG,
        SI,
        VI,
    };
    enum class Level : uint8_t {
        MAX  = 255,
        HIGH = 3,
        MED  = 2,
        LOW  = 1,
        NONE = 0
    };
    enum class Severity {
        INFO,
        WARNING,
        ERROR,
    };

    Logger(std::string_view file) //
        post(m_file != nullptr);

    ~Logger();

    auto enable() -> void;
    auto disable() -> void;

    auto enabled() -> bool;

    auto setLevel(Level level) -> void;
    auto setFilter(Sys sys) -> void;

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

    Level              m_level = Level::HIGH;
    std::FILE*         m_file{};
    std::optional<Sys> m_sys{};
    bool               m_enabled{};
};

// implementation

Logger::Logger(std::string_view file) {
    m_file    = std::fopen(file.data(), "w");
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

auto Logger::setFilter(Sys sys) -> void {
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

// template <Logger::Verbosity v, class T>
//     requires(!Tuple_c<T>)
// auto Logger::log(T obj) -> void {
//     if (!m_enabled || v > m_verbosity) {
//         return;
//     }
//     std::println(m_file,
//                  "{{\"level\": {}, \"{}\": {}}}",
//                  static_cast<int>(v),
//                  std::meta::display_string_of(^^T),
//                  format(obj));
// }

template <Logger::Level Level, Logger::Sys Sys, Tuple_c... Args>
auto Logger::log(Args... args) -> void {
    if (!m_enabled || Level < m_level || (m_sys && Sys != *m_sys)) {
        return;
    }
    auto str = std::string{"{"};
    str += std::format("\"level\": \"{}\"", *Util::enumName(Level));
    if constexpr (Sys != Logger::Sys::NONE) {
        str += std::format(", \"sys\": \"{}\"", *Util::enumName(Sys));
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
}

template <Logger::Level Level, Logger::Severity Sev, Logger::Sys Sys, typename... Args>
auto Logger::log(const char* fmt, Args... args) -> void {
    if (!m_enabled || Level < m_level || (m_sys && Sys != *m_sys)) {
        return;
    }
    auto str = std::format(std::runtime_format(fmt), args...);
    if constexpr (Sys != Logger::Sys::NONE) {
        std::println(m_file,
                     "{{\"level\": \"{}\", \"sys\": \"{}\", \"{}\": \"{}\"}}",
                     *Util::enumName(Level),
                     *Util::enumName(Sys),
                     Util::toLower(*Util::enumName(Sev)),
                     str);
    } else {
        std::println(m_file,
                     "{{\"level\": \"{}\", \"{}\": \"{}\"}}",
                     *Util::enumName(Level),
                     Util::toLower(*Util::enumName(Sev)),
                     str);
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
