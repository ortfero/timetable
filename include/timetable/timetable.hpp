// This file is part of timetable library
// Copyright 2021-2022 Andrei Ilin <ortfero@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once


#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <utility>
#include <vector>


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

    } // namespace detail


    template<typename L = detail::spinlock>
    class scheduler {
    public:
        using lock_type = L;
        using task_id = std::uint64_t;
        using clock_type = std::chrono::system_clock;
        using time_point = typename clock_type::time_point;
        using duration = typename clock_type::duration;
        using days = std::chrono::duration<std::int64_t, std::ratio<86400>>;
        using handler = std::function<void (std::chrono::system_clock::time_point)>;
        

        struct task {
            task_id id;
            time_point next_time;
            duration interval;
            handler handler;
        }; // task

        using task_ptr = std::shared_ptr<task>;
        using tasks = std::multimap<time_point, task_ptr>;
        using rescheduled_tasks = std::vector<task_ptr>;
        
    private:
        
        duration granularity_;
        std::atomic<bool> stopping_{false};
        std::condition_variable cv_;
        std::mutex cv_mutex_;
        std::thread worker_;
        tasks tasks_;
        lock_type tasks_lock_;
        lock_type pass_lock_;
        rescheduled_tasks rescheduled_tasks_;
        std::atomic<task_id> current_task_id_{1};
        
    public:


        scheduler(duration const& granularity = std::chrono::milliseconds{500})
            : granularity_{granularity}
        { }
        
        scheduler(scheduler const&) = delete;
        scheduler& operator = (scheduler const&) = delete;
        ~scheduler() { stop(); }


        template<typename F>
        task_id schedule_from_time(time_point time_at, duration const& interval, F&& handler) {
            auto const next_time = time_at;
            auto const id = current_task_id_.fetch_add(1, std::memory_order_relaxed);
            auto task_ptr = std::make_shared<task>(
                task{id, next_time, interval, std::forward<F>(handler)});
            auto guard = std::unique_lock<lock_type>(tasks_lock_);
            tasks_.insert({next_time, std::move(task_ptr)});
            return id;
        }

        
        template<typename F>
        task_id schedule_from_now(duration const& interval, F&& handler) {
            auto const started = clock_type::now();
            handler(started);
            auto const next_time = started + interval;
            return schedule_from_time(started + interval, interval, std::forward<F>(handler));
        }
                
        
        bool unschedule(task_id tid) noexcept {
            auto guard = std::unique_lock<lock_type>{tasks_lock_};
            for(auto it = tasks_.begin(); it != tasks_.end(); ++it) {
                if(it->id != tid)
                    continue;
                tasks_.erase(it);
                return true;
            }
            return false;
        }

      
        template<typename F>
        task_id schedule_daily_at(duration const& time_of_day, F&& handler) {
            auto const now = clock_type::now();
            auto const day = std::chrono::floor<days>(now);
            auto const today = day + time_of_day;
            auto const interval = std::chrono::hours{24};
            auto const next_time = (now < today) ? today : today + interval;
            return schedule_from_time(next_time, interval, std::forward<F>(handler));
        }
        
        
        template<typename F>
        task_id schedule_every_hour(F&& handler) {
            auto const now = clock_type::now();
            auto const current_hour = std::chrono::floor<std::chrono::hours>(now);
            auto const next_hour = current_hour + std::chrono::hours{1};
            return schedule_from_time(next_hour, std::chrono::hours{1}, std::forward<F>(handler));
        }
        
        
        template<typename F>
        task_id schedule_every_minute(F&& handler) {
            auto const now = clock_type::now();
            auto const current_minute = std::chrono::floor<std::chrono::minutes>(now);
            auto const next_minute = current_minute + std::chrono::minutes{1};
            return schedule_from_time(next_minute, std::chrono::minutes{1}, std::forward<F>(handler));
        }
        
        
        template<typename F>
        task_id schedule_every_second(F&& handler) {
            auto const now = clock_type::now();
            auto const current_second = std::chrono::floor<std::chrono::seconds>(now);
            auto const next_second = current_second + std::chrono::seconds{1};
            return schedule_from_time(next_second, std::chrono::seconds{1}, std::forward<F>(handler));
        }
        
        
        template<typename F>
        task_id schedule_once(time_point time_at, F&& handler) {
            auto const now = clock_type::now();
            return schedule_from_time(time_at, duration::zero(), std::forward<F>(handler));
        }

        
        void pass() {
            auto const scheduled_time = clock_type::now();
            auto guard = std::unique_lock<lock_type>{pass_lock_};
            get_rescheduled_tasks(scheduled_time);
            for(auto& rescheduled: rescheduled_tasks_) {
                rescheduled->handler(scheduled_time);
            }
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
                    pass();
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
    
        void get_rescheduled_tasks(clock_type::time_point scheduled_time) {
            auto guard = std::unique_lock<lock_type>{tasks_lock_};
            auto end_task = tasks_.upper_bound(scheduled_time);
            for(auto it = tasks_.begin(); it != end_task; ++it) {
                auto& task = *it->second;
                task.next_time = task.next_time + task.interval;
                rescheduled_tasks_.push_back(std::move(it->second));
            }
            tasks_.erase(tasks_.begin(), end_task);
            for(auto& rescheduled: rescheduled_tasks_) {
                if(rescheduled->interval == duration::zero())
                    continue;
                tasks_.insert({rescheduled->next_time, rescheduled});
            }
        }
        
    }; // scheduler


} // namespace timetable
