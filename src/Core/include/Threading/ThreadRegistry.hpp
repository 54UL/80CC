#pragma once

#include <Threading/WorkerThread.hpp>
#include <Threading/ThreadPool.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <future>
#include <spdlog/spdlog.h>
#include <future>

namespace ettycc
{
    // ── ThreadRegistry ───────────────────────────────────────────────────────────
    // Central owner of all engine threads.
    //
    // Every thread the engine creates — persistent workers AND one-shot pool
    // tasks — goes through this class.  This gives a single place to:
    //   • query what threads exist and their state (for the debugger UI)
    //   • shut everything down cleanly on exit
    //   • prevent runaway thread creation by future systems
    //
    // Two flavours of concurrency:
    //   1. Named WorkerThreads  — long-lived loops (network poller, etc.)
    //   2. Pool tasks           — fire-and-forget via Submit() → std::future
    //                             (physics step, audio tick, asset loads)
    class ThreadRegistry
    {
    public:
        // poolSize = number of general-purpose worker threads in the pool.
        explicit ThreadRegistry(size_t poolSize = 2)
            : taskPool_(poolSize) {}

        ~ThreadRegistry() { Shutdown(); }

        // Non-copyable.
        ThreadRegistry(const ThreadRegistry&)            = delete;
        ThreadRegistry& operator=(const ThreadRegistry&) = delete;

        // ── Persistent workers ───────────────────────────────────────────────────

        // Create a named worker thread.  Returns a reference to the new worker.
        // Caller is responsible for calling worker.Start(fn) when ready.
        WorkerThread& CreateWorker(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto [it, inserted] = workers_.emplace(
                name, std::make_unique<WorkerThread>(name));
            if (!inserted)
                spdlog::warn("[ThreadRegistry] Worker '{}' already exists — returning existing", name);
            return *it->second;
        }

        // Get a worker by name.  Returns nullptr if not found.
        WorkerThread* GetWorker(const std::string& name)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = workers_.find(name);
            return (it != workers_.end()) ? it->second.get() : nullptr;
        }

        // ── Pool tasks (one-shot) ────────────────────────────────────────────────

        // Submit a task to the thread pool and return a future for the result.
        // This is the replacement for raw std::async calls scattered in Engine.
        template <class F, class... Args>
        auto Submit(F&& f, Args&&... args)
        {
            return taskPool_.enqueue(std::forward<F>(f), std::forward<Args>(args)...);
        }

        // ── Debug / introspection ────────────────────────────────────────────────

        struct ThreadInfo
        {
            std::string          name;
            WorkerThread::State  state;
            float                lastDurationMs;
            size_t               iterations;
        };

        // Snapshot of all managed worker threads.
        std::vector<ThreadInfo> GetStatus() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<ThreadInfo> result;
            result.reserve(workers_.size());
            for (const auto& [name, w] : workers_)
            {
                result.push_back({
                    name,
                    w->GetState(),
                    w->GetLastDurationMs(),
                    w->GetIterationCount()
                });
            }
            return result;
        }

        size_t GetPoolSize() const { return poolThreadCount_; }

        // ── Lifecycle ────────────────────────────────────────────────────────────

        void Shutdown()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [name, w] : workers_)
            {
                if (w->GetState() == WorkerThread::State::Running)
                {
                    spdlog::info("[ThreadRegistry] Stopping worker '{}'", name);
                    w->Stop();
                }
            }
            workers_.clear();
            // ThreadPool destructor joins its own threads.
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, std::unique_ptr<WorkerThread>> workers_;
        ThreadPool taskPool_;
        size_t     poolThreadCount_ = 2;
    };

} // namespace ettycc
