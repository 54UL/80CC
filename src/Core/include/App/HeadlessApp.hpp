#ifndef HEADLESS_APP_HPP
#define HEADLESS_APP_HPP

#include <App/App.hpp>
#include <App/EnginePipeline.hpp>

#include <atomic>
#include <chrono>
#include <memory>

namespace ettycc
{
    // Minimal App implementation for headless (no-GPU) server mode.
    // Runs the engine tick loop at a fixed rate without any window,
    // OpenGL context, or input polling.
    class HeadlessApp : public App
    {
    public:
        explicit HeadlessApp(int tickRate = 60);
        ~HeadlessApp()  = default;

        int  Init(int argc, char** argv) override;
        int  Exec() override;

        float      GetDeltaTime() override;
        float      GetAppTime() override;
        glm::ivec2 GetMainWindowSize() override;

        void AddExecutionPipeline(std::shared_ptr<ExecutionPipeline> pipeline) override;

        void RequestShutdown() { running_.store(false, std::memory_order_release); }
        bool IsRunning() const { return running_.load(std::memory_order_acquire); }

    private:
        int tickRate_;
        float deltaTime_  = 0.f;
        float appTime_    = 0.f;

        std::atomic<bool> running_{true};
        std::shared_ptr<EnginePipeline> engine_;

        using Clock = std::chrono::high_resolution_clock;
        Clock::time_point lastTick_;
    };
} // namespace ettycc

#endif
