#pragma once

#include <atomic>
#include <chrono>
#include <mutex>

class Timer {
public:
    using clock = std::chrono::steady_clock;

    Timer();

    void Start();
    void Pause();
    void Resume();
    void Stop();
    void Reset();

    std::chrono::nanoseconds Elapsed() const;

private:
    mutable std::mutex mutex;

    std::atomic<bool> is_running;
    std::atomic<bool> is_paused;

    clock::time_point start_point;
    std::chrono::nanoseconds elapsed_time;
};
