#define _CRT_SECURE_NO_WARNINGS

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <timetable/timetable.hpp>



TEST_CASE("snippet") {
  timetable::scheduler<> scheduler;
  scheduler.every(std::chrono::seconds{3},
                  [](){});
  scheduler.run();
  scheduler.stop();
}
