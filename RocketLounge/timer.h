#pragma once

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace std;
 
int timestamp() {
    const auto p1 = std::chrono::system_clock::now();
    return chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count();
}

class Timer {
	std::atomic<bool> active{true};
    public:
    template <typename F>
    void setTimeout(F function, int delay);
    template <typename F>
    void setInterval(F function, int interval);
    void stop();

};

template <typename F>
void Timer::setTimeout(F function, int delay) {
    active = true;
    std::thread t([=]() {
        if(!active.load()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if(!active.load()) return;
        function();
    });
    t.detach();
}

template <typename F>
void Timer::setInterval(F function, int interval) {
    active = true;
    std::thread t([=]() {
        while(active.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            if(!active.load()) return;
            function();
        }
    });
    t.detach();
}

void Timer::stop() {
    active = false;
}

Timer _implicitTimer;
template <typename F>
void setTimeout(F function, int delay) {
    _implicitTimer.setTimeout(function, delay);
}

