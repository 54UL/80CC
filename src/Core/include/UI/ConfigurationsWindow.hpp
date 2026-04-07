#pragma once
#include <Build/BuildConfig.hpp>
#include <vector>
#include <string>

namespace ettycc
{
    class ConfigurationsWindow
    {
    public:
        ConfigurationsWindow();

        void Open()         { open_ = true; }
        bool IsOpen() const { return open_; }
        void Draw(); // no-op when closed

        build::GlobalBuildConfig& GetBuildConfig() { return buildConfig_; }

    private:
        bool open_           = false;
        bool autoDetectDone_ = false;
        build::GlobalBuildConfig buildConfig_;
        std::string defaultConfigPath_;

        enum class Category { Build, Globals };
        Category selectedCategory_ = Category::Build;
        char globalsFilter_[256]   = {};

        // Persistence
        void SaveConfig(const std::string& path);
        bool LoadConfig(const std::string& path);

        void DrawBuildSettings();
        void DrawGlobals();
    };
} // namespace ettycc
