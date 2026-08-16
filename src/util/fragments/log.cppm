module;
import std;
export module Util:Log;

using namespace std::string_view_literals;

template <typename T>
concept Tuple_c = requires { std::tuple_size<T>::value; };

export namespace Util {

class Logger {
  public:
    enum class Verbosity {
        MAX  = 3,
        HIGH = 2,
        MED  = 1,
        LOW  = 0,
    };

    Logger(std::string_view file) //
        post(m_file != nullptr);

    ~Logger();

    auto enable() -> void;
    auto disable() -> void;

    auto enabled() -> bool;

    auto setVerbosity(Verbosity v) -> void;

    template <typename... Args>
    auto logUnstructured(const char* fmt, Args... obj) -> void;

    template <Verbosity v, Tuple_c... Args>
    auto log(Args... args) -> void;

    template <Verbosity v, class T>
    auto log(T obj) -> void;

    template <Verbosity v>
    auto log(std::string_view msg) -> void;

    template <Verbosity v, typename... Args>
    auto log(const char* fmt, Args... obj) -> void;

    auto flush() -> void;

  private:
    template <class T>
    auto format(T obj) -> std::string;

    Verbosity  m_verbosity = Verbosity::MED;
    std::FILE* m_file;
    bool       m_enabled;
};

using Verbosity = Util::Logger::Verbosity;

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

auto Logger::setVerbosity(Verbosity v) -> void {
    m_verbosity = v;
}

auto Logger::flush() -> void {
    if (!m_enabled) {
        return;
    }
    std::fflush(m_file);
}

template <typename... Args>
auto Logger::logUnstructured(const char* fmt, Args... args) -> void {
    if (!m_enabled) {
        return;
    }
    std::println(m_file, std::runtime_format(fmt), args...);
}

template <Logger::Verbosity v, class T>
auto Logger::log(T obj) -> void {
    if (!m_enabled || v > m_verbosity) {
        return;
    }
    std::println(m_file,
                 "{{\"level\": {}, \"{}\": {}}}",
                 static_cast<int>(v),
                 std::meta::display_string_of(^^T),
                 format(obj));
}

template <Logger::Verbosity v, Tuple_c... Args>
auto Logger::log(Args... args) -> void {
    if (!m_enabled || v > m_verbosity) {
        return;
    }
    auto str = std::string{"{"};
    str += std::format("\"level\": {}", static_cast<int>(v));
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

template <Logger::Verbosity v>
auto Logger::log(std::string_view msg) -> void {
    if (!m_enabled || v > m_verbosity) {
        return;
    }
    std::println(m_file,
                 "{{\"level\": {}, \"message\": \"{}\"}}",
                 static_cast<int>(v),
                 msg);
}

template <Logger::Verbosity v, typename... Args>
auto Logger::log(const char* fmt, Args... args) -> void {
    if (!m_enabled || v > m_verbosity) {
        return;
    }
    auto str = std::format(std::runtime_format(fmt), args...);
    log<v>(std::string_view{str});
}

template <class T>
auto Logger::format(T obj) -> std::string {
    if constexpr (std::formattable<T, char>) {
        return std::format("\"{}\"", std::format("{}", obj)); // TODO
    }
    return std::format("\"{}\"", std::meta::display_string_of(^^T)); // TODO
}

} // namespace Util
