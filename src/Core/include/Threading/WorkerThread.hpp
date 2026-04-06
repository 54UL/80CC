#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

namespace ettycc
{
    // ── WorkerThread ─────────────────────────────────────────────────────────────
    // A named persistent thread that runs a user-provided function in a loop.
    //
    // The work function should be short-lived per iteration (non-blocking or
    // bounded-time).  The worker calls it repeatedly until Stop() is called.
    //
    // Lifecycle:  Idle → Running → Stopping → Stopped
    //             Start()         Stop()
    class WorkerThread
    {
    public:
        enum class State : uint8_t { Idle, Running, Stopping, Stopped };

        explicit WorkerThread(std::string name)
            : name_(std::move(name)) {}

        ~WorkerThread() { Stop(); }

        // Non-copyable, non-movable (owns a thread).
        WorkerThread(const WorkerThread&)            = delete;
        WorkerThread& operator=(const WorkerThread&) = delete;
        WorkerThread(WorkerThread&&)                 = delete;
        WorkerThread& operator=(WorkerThread&&)      = delete;

        // Start the worker. workFn is called in a loop until Stop().
        void Start(std::function<void()> workFn)
        {
            State expected = State::Idle;
            if (!state_.compare_exchange_strong(expected, State::Running))
            {
                // Also allow restart from Stopped
                expected = State::Stopped;
                if (!state_.compare_exchange_strong(expected, State::Running))
                    return;
            }

            workFn_ = std::move(workFn);
            thread_ = std::thread([this]() { Run(); });
        }

        void Stop()
        {
            State expected = State::Running;
            if (!state_.compare_exchange_strong(expected, State::Stopping))
                return;

            if (thread_.joinable())
                thread_.join();

            state_.store(State::Stopped, std::memory_order_release);
        }

        // ── Accessors ────────────────────────────────────────────────────────────
        const std::string& GetName()  const { return name_; }
        State  GetState()             const { return state_.load(std::memory_order_acquire); }
        float  GetLastDurationMs()    const { return lastDurationMs_.load(std::memory_order_relaxed); }
        size_t GetIterationCount()    const { return iterations_.load(std::memory_order_relaxed); }

        const char* GetStateStr() const
        {
            switch (GetState())
            {
            case State::Idle:     return "Idle";
            case State::Running:  return "Running";
            case State::Stopping: return "Stopping";
            case State::Stopped:  return "Stopped";
            }
            return "Unknown";
        }

    private:
        void Run()
        {
            using Clock = std::chrono::high_resolution_clock;
            using Ms    = std::chrono::duration<float, std::milli>;

            while (state_.load(std::memory_order_acquire) == State::Running)
            {
                auto t0 = Clock::now();
                workFn_();
                auto t1 = Clock::now();

                lastDurationMs_.store(Ms(t1 - t0).count(), std::memory_order_relaxed);
                iterations_.fetch_add(1, std::memory_order_relaxed);
            }
        }

        std::string              name_;
        std::atomic<State>       state_{State::Idle};
        std::function<void()>    workFn_;
        std::thread              thread_;
        std::atomic<float>       lastDurationMs_{0.f};
        std::atomic<size_t>      iterations_{0};
    };

} // namespace ettycc
