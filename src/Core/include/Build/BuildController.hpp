#pragma once
#include <Build/BuildConfig.hpp>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace ettycc::build
{
    struct BuildRequest
    {
        std::string sourcePath;
        std::string outputPath;
        std::string projectName;
        std::string cfgStr;
        bool doClean       = false;
        bool keepGenerated = false;
    };

    // Free function: fills empty fields of cfg using OS auto-detection (Windows only).
    void AutoDetectBuildConfig(GlobalBuildConfig& cfg);

    class BuildController
    {
    public:
        BuildController();
        ~BuildController();

        void StartPipeline(const BuildRequest& req, const GlobalBuildConfig& cfg);

        bool  IsRunning()  const { return running_.load(); }
        float GetProgress() const { return progress_.load(); }
        std::vector<std::string> GetLogSnapshot();
        bool                     ConsumeScrollRequest();
        std::string              GetStatus();
        void                     ClearLog();

    private:
        std::thread              buildThread_;
        std::atomic<bool>        running_       {false};
        std::atomic<float>       progress_      {0.0f};
        std::atomic<bool>        scrollToBottom_{false};
        std::mutex               logMutex_;
        std::vector<std::string> log_;
        std::string              statusMsg_;

        void PushLog(std::string msg);
        void SetStatus(std::string s);
        void RunPipeline(BuildRequest req, GlobalBuildConfig cfg);
    };

} // namespace ettycc::build
