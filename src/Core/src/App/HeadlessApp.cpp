#include <App/HeadlessApp.hpp>
#include <Dependency.hpp>
#include <Engine.hpp>
#include <spdlog/spdlog.h>

#include <thread>
#include <csignal>

namespace ettycc
{
    // ── Global shutdown hook for SIGINT / SIGTERM ────────────────────────────
    static std::atomic<bool>* g_running = nullptr;

    static void SignalHandler(int sig)
    {
        spdlog::warn("[HeadlessApp] Signal {} received — shutting down...", sig);
        if (g_running)
            g_running->store(false, std::memory_order_release);
    }

    HeadlessApp::HeadlessApp(int tickRate)
        : tickRate_(tickRate)
    {
    }

    int HeadlessApp::Init(int argc, char** argv)
    {
        // Install signal handlers for graceful shutdown
        g_running = &running_;
        std::signal(SIGINT,  SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        engine_ = GetDependency(Engine);
        engine_->Init();

        lastTick_ = Clock::now();

        spdlog::info("[HeadlessApp] Initialized — tick rate: {} Hz", tickRate_);
        return 0;
    }

    int HeadlessApp::Exec()
    {
        using namespace std::chrono;

        const auto tickInterval = duration_cast<Clock::duration>(
            duration<double>(1.0 / tickRate_));

        spdlog::info("[HeadlessApp] Entering server loop...");

        while (running_.load(std::memory_order_acquire))
        {
            auto now = Clock::now();
            deltaTime_ = duration<float>(now - lastTick_).count();
            appTime_  += deltaTime_;
            lastTick_  = now;

            engine_->Update();
            // No PrepareFrame / PresentFrame — headless has no rendering

            // Sleep to maintain tick rate
            auto elapsed = Clock::now() - now;
            if (elapsed < tickInterval)
                std::this_thread::sleep_for(tickInterval - elapsed);
        }

        spdlog::info("[HeadlessApp] Server loop exited.");
        return 0;
    }

    float HeadlessApp::GetDeltaTime()
    {
        return deltaTime_;
    }

    float HeadlessApp::GetAppTime()
    {
        return appTime_;
    }

    glm::ivec2 HeadlessApp::GetMainWindowSize()
    {
        // No window — return a dummy size for any code that queries it
        return {0, 0};
    }

    void HeadlessApp::AddExecutionPipeline(std::shared_ptr<ExecutionPipeline> /*pipeline*/)
    {
        // No-op — headless mode doesn't support execution pipelines (editor UI, etc.)
        spdlog::warn("[HeadlessApp] AddExecutionPipeline ignored in headless mode");
    }
} // namespace ettycc
