# timetable
C++17 one-header library to invoke something periodically or at specified time

## Synopsis

```cpp
namespace timetable {

    struct context {
        virtual ~context() { }
    };
    

    template<typename L = detail::spinlock>
    class scheduler {
    public:
        using handler = std::function<void (std::chrono::system_clock::time_point, context* context)>;

        scheduler(duration const& granularity = std::chrono::milliseconds{500});
        
        scheduler(scheduler const&) = delete;
        scheduler& operator = (scheduler const&) = delete;
        ~scheduler();

        template<typename F>
        task_id schedule_from_time(time_point time_at,
                                   duration const& interval,
                                   F&& handler,
                                   std::unique_ptr<context> context = nullptr);
        
        template<typename F>
        task_id schedule_from_now(duration const& interval,
                                  F&& handler,
                                  std::unique_ptr<context> context = nullptr);
        
        
        bool unschedule(task_id tid) noexcept;
      
        template<typename F>
        task_id schedule_daily_at(duration const& time_of_day,
                                  F&& handler,
                                  std::unique_ptr<context> context = nullptr);
        
        template<typename F>
        task_id schedule_every_hour(F&& handler,
                                    std::unique_ptr<context> context = nullptr);
        
        template<typename F>
        task_id schedule_every_minute(F&& handler,
                                      std::unique_ptr<context> context = nullptr);
        
        template<typename F>
        task_id schedule_every_second(F&& handler,
                                      std::unique_ptr<context> context = nullptr);
                
        template<typename F>
        task_id schedule_once(time_point time_at,
                              F&& handler,
                              std::unique_ptr<context> context = nullptr);
        
        void pass(); // pass scheduling
        void run(); // run in a separate thread
        void stop();
    }; // scheduler
    
} // namespace timetable
```

## Snippets


### Set scheduler precision

```cpp
#include <chrono>
#include <timetable/timetable.hpp>
...
auto scheduler = timetable::scheduler{std::chrono::milliseconds{300}};
```

### Run task periodically from now

```cpp
#include <cstdio>
#include <chrono>
#include <thread>
#include <timetable/timetable.hpp>


int main() {
    auto scheduler = timetable::scheduler{};
    scheduler.schedule_from_now(std::chrono::seconds{1},
        [](std::chrono::system_clock::time_point){ std::puts("one second"); });
    scheduler.run();
    std::this_thread::sleep_for(std::chrono::seconds{10});
    scheduler.stop();
    return 0;
}
```

### Run task daily at specified time

```cpp
#include <cstdio>
#include <chrono>
#include <thread>
#include <timetable/timetable.hpp>

int main() {
    auto scheduler = timetable::scheduler{};
    using namespace std::chrono;
    auto const at = hours{13} + minutes{13} + seconds{13};
    scheduler.schedule_daily_at(at,
        [](std::chrono::system_clock::time_point){ std::puts("now 13:13:13 utc"); });
    scheduler.run();
    std::this_thread::sleep_for(minutes{5});
    scheduler.stop();
    return 0;
}
```


### Remove scheduled task
```cpp
#include <chrono>
...
auto scheduler = timetable::scheduler{};

auto const task_id = scheduler.schedule_from_now(std::chrono::hours{1},
    [](std::chrono::system_clock::time_point){ std::puts("1 hour passed"); });
scheduler.unschedule(task_id);
```
