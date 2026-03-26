#ifndef BUILD_PANEL_HPP
#define BUILD_PANEL_HPP

// BuildPanel — self-contained Build window.
//
// Owns all build state, drives the three-stage pipeline:
//   1. ettycc::build::RunGenerator  (inline, no subprocess)
//   2. cmake configure              (subprocess)
//   3. cmake --build                (subprocess)
//
// Call Draw() once per ImGui frame while the "Build" window is active.

#include <Build/ProjectGenerator.hpp>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>

namespace ettycc
{
    class BuildPanel
    {
    public:
        BuildPanel();
        ~BuildPanel();

        void Draw();

    private:
        // ── Path pickers ───────────────────────────────────────────────────────
        char projectName_[256]    = "MyGame";
        char sourcePath_[512]     = {};   // user game source (must contain include/ and src/)
        char outputPath_[512]     = {};   // where to write the generated project
        char coreLibPath_[512]    = {};   // pre-built 80CC_CORE.lib / lib80CC_CORE.a
        char coreIncludePath_[512] = {};  // Core include/ directory

        // ── Build config ───────────────────────────────────────────────────────
        int  configIndex_         = 0;   // index into configs[] array in Draw()
        bool cleanBuild_          = false;
        char cmakeGenerator_[128]  = {};  // e.g. "Ninja" or "Visual Studio 17 2022"
        char vcvarsallPath_[512]   = {};  // path to vcvarsall.bat (auto-detected on Windows)
        char vcpkgToolchain_[512]  = {};  // path to vcpkg.cmake toolchain (auto-detected)

        // ── Pipeline thread ────────────────────────────────────────────────────
        std::thread         buildThread_;
        std::atomic<bool>   running_{false};
        std::atomic<float>  progress_{0.0f};
        std::atomic<bool>   scrollToBottom_{false};

        std::mutex               logMutex_;
        std::vector<std::string> log_;
        std::string              statusMsg_;

        // ── Helpers ────────────────────────────────────────────────────────────
        void RunPipeline(std::string srcPath, std::string outPath,
                         std::string projName, std::string cfgStr, bool doClean,
                         std::string coreLib, std::string coreInc,
                         std::string generator, std::string vcvarsall,
                         std::string vcpkgToolchain);
    };
} // namespace ettycc

#endif
