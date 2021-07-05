#define _CRT_SECURE_NO_WARNINGS

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <timetable/timetable.hpp>



TEST_CASE("snippet") {
    using namespace std::chrono;
    timetable::scheduler scheduler{std::chrono::milliseconds{500}};
    
    scheduler.every(seconds{10},
                    [](){ std::puts("every second"); });
    scheduler.daily_at_utc(hours{12} + minutes{12},
                    [](){ std::puts("at 12:12:00 UTC"); });

    scheduler.run();
    std::this_thread::sleep_for(std::chrono::minutes{3});
    scheduler.stop();
}
