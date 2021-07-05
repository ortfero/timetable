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


template<typename L = detail::spinlock>
class scheduler {
public:
    using lock_type = L;
    using clock_type = std::chrono::system_clock;
    using time_point = typename clock_type::time_point;
    using duration = typename clock_type::duration;
    using days = std::chrono::duration<std::int64_t, std::ratio<86400>>;

    struct task {
        time_point previous;
        duration interval;
        time_point next;
        std::function<void ()> callable;
    }; // task

    using task_ptr = std::unique_ptr<task>;
    using tasks = std::multimap<time_point, task_ptr>;
    using rescheduled_tasks = std::vector<task_ptr>;

    scheduler(duration const& granularity = std::chrono::seconds{1})
        : granularity_{granularity}
    { }
    
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
        tasks_.insert({ next, std::move(tp) });
        return true;
    }

  
    template<typename F>
    bool daily_at_utc(duration const& at_time, F&& f) {

        time_point const now = clock_type::now();
        time_point const day = std::chrono::floor<days>(now);
        time_point const today = day + at_time;
        duration const interval = std::chrono::hours{24};
        time_point const next = (now < today) ? today : today + interval;
        task_ptr tp = std::make_unique<task>(task{time_point(), interval,
                                                next, std::forward<F>(f)});
        std::unique_lock<lock_type> g(tasks_lock_);
        tasks_.insert({next, std::move(tp)});
    
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
                auto const r = cv_.wait_for(g, granularity_);
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
    
    duration granularity_;
    std::atomic<bool> stopping_{false};
    std::condition_variable cv_;
    std::mutex cv_mutex_;
    std::thread worker_;
    tasks tasks_;
    lock_type tasks_lock_;
    rescheduled_tasks rescheduled_tasks_;
    
}; // scheduler


} // timetable
