#include "utils/Timer.hpp"

Timer::Timer() :
    is_running(false),
    is_paused(false),
    elapsed_time(0)
{}

void Timer::Start() {
    std::lock_guard<std::mutex> lock(mutex);

    if (is_running) {
        return;
    }

    is_running = true;
    is_paused = false;
    elapsed_time = std::chrono::nanoseconds(0);
    start_point = clock::now();
}

void Timer::Pause() {
    std::lock_guard<std::mutex> lock(mutex);

    if (!is_running || is_paused) {
        return;
    }

    elapsed_time += clock::now() - start_point;
    is_paused = true;
}

void Timer::Resume() {
    std::lock_guard<std::mutex> lock(mutex);

    if (!is_running || !is_paused) {
        return;
    }

    start_point = clock::now();
    is_paused = false;
}

void Timer::Stop() {
    std::lock_guard<std::mutex> lock(mutex);

    if (!is_running) {
        return;
    }

    if (!is_paused) {
        elapsed_time += clock::now() - start_point;
    }

    is_running = false;
    is_paused = false;
}

void Timer::Reset() {
    std::lock_guard<std::mutex> lock(mutex);

    is_running = false;
    is_paused = false;
    elapsed_time = std::chrono::nanoseconds(0);
}

std::chrono::nanoseconds Timer::Elapsed() const {
    if (!is_running.load()) {
        return elapsed_time;
    }

    if (is_paused.load()) {
        return elapsed_time;
    }

    return elapsed_time + (clock::now() - start_point);
}
