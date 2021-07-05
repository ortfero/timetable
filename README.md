# timetable
C++17 one-header library to invoke something periodically or at specified time


## Snippets

### Add periodical callback

```cpp
#include <cstdio>
#include <chrono>
#include <thread>
#include <timetable/timetable.hpp>


int main() {
    timetable::scheduler scheduler;
    scheduler.every(std::chrono::seconds{1},
        [](){ std::puts("one second"); });
    scheduler.run();
    std::this_thread::sleep_for(std::chrono::seconds{10});
    scheduler.stop();
    return 0;
}
```

### Add daily callback

```cpp
#include <cstdio>
#include <chrono>
#include <thread>
#include <timetable/timetable.hpp>

int main() {
    timetable::scheduler scheduler;
    using namespace std::chrono;
    auto const at = hours{13} + minutes{13} + seconds{13};
    scheduler.daily_at_utc(at, [](){ std::puts("now 13:13:13 utc"); });
    scheduler.run();
    std::this_thread::sleep_for(minutes{5});
    scheduler.stop();
    return 0;
}
```

### Set precision
```cpp
#include <chrono>

int main() {
    // 1 second by default
    timetable::scheduler scheduler{std::chrono::milliseconds{500}};
    return 0;
}
```
