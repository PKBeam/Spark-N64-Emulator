export module Util:Timer;

import std;

export namespace Util {

template <std::size_t Frequency>
class Timer {
    using ClockType = std::chrono::steady_clock;
    using Interval  = std::chrono::duration<std::chrono::milliseconds::rep, std::ratio<1, Frequency>>;

  public:
    Timer(std::function<void()> callback) {
        m_nextTick = std::chrono::time_point_cast<Interval>(ClockType::now() + Interval(1));
        m_thread   = std::jthread([this, callback](std::stop_token stoken) {
            while (!stoken.stop_requested()) {
                if (ClockType::now() >= m_nextTick) {
                    callback();
                    m_nextTick = std::chrono::time_point_cast<Interval>(m_nextTick + Interval(1));
                }
            }
        });
    }

    auto stop() -> void {
        m_thread.request_stop();
        m_thread.join();
    }

  private:
    std::chrono::time_point<ClockType, Interval> m_nextTick{};
    std::jthread                                 m_thread;
};

} // namespace Util
