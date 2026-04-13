#pragma once

#include <Threading/WorkerThread.hpp>
#include <Threading/ThreadPool.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <future>
#include <spdlog/spdlog.h>

#undef min
#undef max

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
        // Default: hardware threads minus 2 (main + network), minimum 2.
        explicit ThreadRegistry(size_t poolSize = 0)
            : taskPool_(poolSize > 0
                        ? poolSize
                        : std::max<size_t>(2, std::thread::hardware_concurrency() > 2
                                              ? std::thread::hardware_concurrency() - 2
                                              : 2))
            , poolThreadCount_(poolSize > 0
                               ? poolSize
                               : std::max<size_t>(2, std::thread::hardware_concurrency() > 2
                                                     ? std::thread::hardware_concurrency() - 2
                                                     : 2))
        {
            spdlog::info("[ThreadRegistry] Pool created with {} threads (hw: {})",
                         poolThreadCount_, std::thread::hardware_concurrency());
        }

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

        // ── Parallel iteration ─────────────────────────────────────────────────────

        // Split [0, count) into chunks across pool threads and run fn(begin, end)
        // on each chunk.  Blocks until all chunks complete.
        // Falls back to single-threaded if count is below threshold.
        template <typename Fn>
        void ParallelFor(size_t count, Fn&& fn, size_t minPerThread = 64)
        {
            if (count == 0) return;

            const size_t maxChunks = poolThreadCount_;
            const size_t numChunks = (count >= minPerThread * 2 && maxChunks > 1)
                                   ? std::min(maxChunks, (count + minPerThread - 1) / minPerThread)
                                   : 1;

            if (numChunks <= 1)
            {
                fn(size_t(0), count);
                return;
            }

            const size_t chunkSize = (count + numChunks - 1) / numChunks;
            std::vector<std::future<void>> futures;
            futures.reserve(numChunks - 1);

            // Submit N-1 chunks to pool, run last chunk on caller's thread.
            for (size_t c = 0; c < numChunks - 1; ++c)
            {
                const size_t begin = c * chunkSize;
                const size_t end   = std::min(begin + chunkSize, count);
                futures.push_back(taskPool_.enqueue([&fn, begin, end]() {
                    fn(begin, end);
                }));
            }

            // Last chunk runs on the calling thread (no pool overhead).
            {
                const size_t begin = (numChunks - 1) * chunkSize;
                const size_t end   = count;
                fn(begin, end);
            }

            for (auto& f : futures) f.get();
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
        size_t     poolThreadCount_;
    };

} // namespace ettycc
