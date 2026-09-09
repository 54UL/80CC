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
        ~ConfigurationsWindow();

        void Open()         { open_ = true; }
        bool IsOpen() const { return open_; }
        void Draw(); // no-op when closed

        build::GlobalBuildConfig& GetBuildConfig() { return buildConfig_; }

    private:
        bool open_           = false;
        bool autoDetectDone_ = false;
        build::GlobalBuildConfig buildConfig_;
        std::string defaultConfigPath_;

        enum class Category { Build, Globals, Physics, Rendering };
        Category selectedCategory_ = Category::Build;
        char globalsFilter_[256]   = {};
        int  physicsSelected_      = -1;

        // Rendering layers
        char newLayerName_[64] = {};

        // Persistence
        void SaveConfig(const std::string& path);
        bool LoadConfig(const std::string& path);

        void DrawBuildSettings();
        void DrawGlobals();
        void DrawPhysics();
        void DrawRendering();
    };
} // namespace ettycc
