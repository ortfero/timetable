#define _CRT_SECURE_NO_WARNINGS

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <timetable/timetable.hpp>



TEST_CASE("snippet") {
    using namespace std::chrono;
    auto scheduler = timetable::scheduler{};
    
    /*scheduler.schedule_from_now(seconds{3},
        [](auto){ std::puts("every 3 seconds"); });
    scheduler.schedule_daily_at(hours{12} + minutes{20},
        [](auto){ std::puts("at 12:20:00 UTC"); });*/
    scheduler.schedule_every_second(
        [](auto, auto*) { std::puts("every second"); });
    scheduler.schedule_every_minute(
        [](auto, auto*) { std::puts("every minute"); });
    scheduler.schedule_once(std::chrono::system_clock::now() + std::chrono::seconds{3},
        [](auto, auto*) { std::puts("once"); });

    scheduler.run();
    std::this_thread::sleep_for(std::chrono::minutes{3});
    scheduler.stop();
}
