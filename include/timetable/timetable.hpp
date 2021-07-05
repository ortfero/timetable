#pragma once


#include <chrono>
#include <functional>
#include <utility>
#include <memory>
#include <vector>
#include <atomic>
#include <map>
#include <thread>
#include <condition_variable>
#include <time.h>


namespace timetable {
    
    
namespace detail {
    

class spinlock {
public:

    spinlock() noexcept = default;
    spinlock(spinlock const&) noexcept = delete;
    spinlock& operator = (spinlock const&) noexcept = delete;


    bool try_lock() noexcept {
        if(flag_.load(std::memory_order_relaxed))
            return false;

        return !flag_.exchange(true, std::memory_order_acquire);
    }


    void unlock() noexcept {
        flag_.store(false, std::memory_order_release);
    }
  

    void lock() noexcept {
        while(!try_lock())
            std::this_thread::yield();
    }


private:

    alignas(64)
        std::atomic_bool flag_{false};

}; // spinlock

} // detail
  

struct scheduler_traits {
    using lock_type = detail::spinlock;
    using clock_type = std::chrono::system_clock;
    static inline typename clock_type::duration const precision = std::chrono::seconds(1);
}; // scheduler_traits


template<typename Tr = scheduler_traits>
class scheduler {
public:
    using lock_type = typename Tr::lock_type;
    using clock_type = typename Tr::clock_type;
    using time_point = typename clock_type::time_point;
    using duration = typename clock_type::duration;

    struct task {
        time_point previous;
        duration interval;
        time_point next;
        std::function<void ()> callable;
    }; // task

    using task_ptr = std::unique_ptr<task>;
    using tasks = std::multimap<time_point, task_ptr>;
    using rescheduled_tasks = std::vector<task_ptr>;

    scheduler() = default;
    scheduler(scheduler const&) = delete;
    scheduler& operator = (scheduler const&) = delete;
    ~scheduler() { stop(); }

    
    template<typename F>
    bool every(duration const& interval, F&& f) {
        time_point const now = clock_type::now();
        f();
        time_point const next = now + interval;
        task_ptr tp = std::make_unique<task>(task{now, interval,
                                                next, std::forward<F>(f)});
        std::unique_lock<lock_type> g(tasks_lock_);
        tasks_.insert({ next, move(tp) });
        return true;
    }

  
    template<typename F>
    bool daily(int hour, int minute, int second, F&& f) {
        time_t const now = time(nullptr);
        tm current;
#if _WIN32
        if(!gmtime_s(&current, &now))
#else
        if(!gmtime_r(&now, &current))
#endif
            return false;

        current.tm_hour = hour;
        current.tm_min = minute;
        current.tm_sec = second;
        time_t const today = mktime(&current);
        if(today == -1)
            return false;
        if(now == today)
            f();
        duration const interval = std::chrono::hours(24);
        time_point const today_point = clock_type::from_time_t(today);
        time_point const next = (now < today) ? today_point : today_point + interval;
        task_ptr tp = std::make_unique<task>(task{time_point(), interval,
                                                next, std::forward<F>(f)});
        std::unique_lock<lock_type> g(tasks_lock_);
        tasks_.insert(next, move(tp));
    
        return true;
    }

    
    void schedule() {
        time_point const scheduled = clock_type::now();
        std::unique_lock<lock_type> g(tasks_lock_);
        if(tasks_.empty()) return;
        auto last = tasks_.upper_bound(scheduled);
        for(auto it = tasks_.begin(); it != last; ++it) {
            time_point const now = clock_type::now();
            it->second->callable();
            it->second->previous = now;
            time_point const next = scheduled + it->second->interval;
            it->second->next = next;
            rescheduled_tasks_.push_back(std::move(it->second));
        }
        tasks_.erase(tasks_.begin(), last);
        for(auto& rescheduled: rescheduled_tasks_)
            tasks_.insert({rescheduled->next, std::move(rescheduled)});
        rescheduled_tasks_.clear();
    }

  
    void run() {
        if(worker_.joinable())
            return;
        worker_ = std::thread([&]() {
            while(!stopping_) {
                std::unique_lock<std::mutex> g(cv_mutex_);
                auto const r = cv_.wait_for(g, Tr::precision);
                if(r != std::cv_status::timeout)
                    continue;
                schedule();
            }
                stopping_ = false;
        });
    }

  
    void stop() {
        if(!worker_.joinable() || stopping_)
            return;
        stopping_ = true;
        cv_.notify_one();
        worker_.join();
    }

private:
    
    std::atomic<bool> stopping_{false};
    std::condition_variable cv_;
    std::mutex cv_mutex_;
    std::thread worker_;
    tasks tasks_;
    lock_type tasks_lock_;
    rescheduled_tasks rescheduled_tasks_;
}; // scheduler


} // timetable
