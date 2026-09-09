#pragma once
#include <Build/BuildConfig.hpp>
#include <Build/BuildController.hpp>
#include <vector>
#include <string>

namespace ettycc
{
    class BuildPanelUI
    {
    public:
        explicit BuildPanelUI(build::GlobalBuildConfig& globalCfg);
        ~BuildPanelUI();
        void Draw();
        void Open()         { open_ = true; }
        bool IsOpen() const { return open_; }

    private:
        build::GlobalBuildConfig& globalCfg_;
        build::BuildController    controller_;

        // Per-build state
        char projectName_[256] = "MyGame";
        char sourcePath_ [512] = {};
        char outputPath_ [512] = {};
        int  configIndex_      = 0;
        bool open_             = false;
        bool cleanBuild_       = false;
        bool keepGenerated_    = false;

        bool defaultsApplied_  = false;  // pull working_dir / exe_dir from Globals once
        bool wasRunning_       = false;  // used to detect build-complete transition
    };
} // namespace ettycc
