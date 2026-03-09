#pragma once

#include <chrono>
#include <mutex>

class Timer {
public:
    Timer();

    void Start();
    void Pause();
    void Resume();
    void Stop();
    void Reset();

    std::chrono::nanoseconds Elapsed() const;

private:
    using clock = std::chrono::steady_clock;

    bool is_running;
    bool is_paused;

    mutable std::mutex mutex;
    clock::time_point start_point;
    std::chrono::nanoseconds elapsed_time;
};
