#pragma once

// Header-only helper that rebuilds all loaded game-module DLLs.
//
// Responsibilities:
//   1. Gather cmake target names from the engine's ModuleLoader
//      (each loaded DLL stem = cmake target name).
//   2. Find the cmake build directory by walking up from the executable.
//   3. Run  cmake --build <dir> --config <cfg> --target A B C --parallel
//      on a background thread, piping output to spdlog.
//   4. Set atomic flags so the caller (DevEditor) knows when to
//      call ModuleLoader::ForceReloadAll().
//
// No engine dependencies beyond forward declarations — only std:: + spdlog.

#include <Build/BuildConfig.hpp>
#include <Build/BuildController.hpp>

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ettycc::build
{

// ── Helpers ──────────────────────────────────────────────────────────────────

// Extract cmake target names from the list of loaded module DLL source paths.
// e.g.  "D:/…/assets/modules/SampleModule.dll"  →  "SampleModule"
inline std::vector<std::string> GatherModuleTargets(
    const std::vector<std::string>& dllSourcePaths)
{
    std::vector<std::string> targets;
    targets.reserve(dllSourcePaths.size());
    for (const auto& p : dllSourcePaths)
    {
        auto stem = std::filesystem::path(p).stem().string();
        if (!stem.empty())
            targets.push_back(std::move(stem));
    }
    return targets;
}

// Locate the cmake build directory by walking up from the running executable.
// Returns empty string on failure.
inline std::string FindBuildDirectory()
{
    namespace fs = std::filesystem;
#ifdef _WIN32
    char exePath[512] = {};
    if (!GetModuleFileNameA(nullptr, exePath, sizeof(exePath)))
        return {};

    fs::path dir = fs::path(exePath).parent_path();
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth, dir = dir.parent_path())
    {
        if (fs::exists(dir / "CMakeCache.txt"))
            return dir.string();
    }
#endif
    return {};
}

// Detect the build configuration from the exe's immediate parent directory.
// Falls back to "Release" if the directory name isn't a standard config.
inline std::string DetectBuildConfig()
{
#ifdef _WIN32
    char exePath[512] = {};
    if (GetModuleFileNameA(nullptr, exePath, sizeof(exePath)))
    {
        std::string dirName = std::filesystem::path(exePath)
                                  .parent_path().filename().string();
        if (dirName == "Debug" || dirName == "Release" ||
            dirName == "RelWithDebInfo" || dirName == "MinSizeRel")
            return dirName;
    }
#endif
    return "Release";
}

// Build the full cmake command line targeting the given modules.
inline std::string BuildCmakeCommand(const std::string& buildDir,
                                     const std::string& buildCfg,
                                     const std::vector<std::string>& targets)
{
    std::string cmd = "cmake --build \"" + buildDir + "\" --config " + buildCfg;
    for (const auto& t : targets)
        cmd += " --target " + t;
    cmd += " --parallel";
    return cmd;
}

// ── ModuleBuildHelper ────────────────────────────────────────────────────────
// Fire-and-forget background build.  The caller polls isRunning() and
// reloadPending() each frame.

class ModuleBuildHelper
{
public:
    ModuleBuildHelper() = default;

    bool IsRunning()       const { return running_.load(); }
    bool IsReloadPending() const { return reloadPending_.load(); }
    void ConsumeReload()         { reloadPending_.store(false); }

    // Kick off a background cmake build for the given module targets.
    // Returns false immediately if a build is already in progress or
    // if the build directory / targets can't be resolved.
    bool Start(const std::vector<std::string>& dllSourcePaths,
               GlobalBuildConfig& cfg)
    {
        if (running_.load()) return false;

        const std::string buildDir = FindBuildDirectory();
        if (buildDir.empty())
        {
            spdlog::error("[ModuleBuild] Cannot find cmake build directory — "
                          "CMakeCache.txt not found above executable path");
            return false;
        }

        auto targets = GatherModuleTargets(dllSourcePaths);
        if (targets.empty())
        {
            spdlog::warn("[ModuleBuild] No module targets to build");
            return false;
        }

        AutoDetectBuildConfig(cfg);

        const std::string buildCfg = DetectBuildConfig();
        const std::string vcvarsall(cfg.vcvarsallPath);
        const std::string buildCmd = BuildCmakeCommand(buildDir, buildCfg, targets);

        spdlog::info("[ModuleBuild] {}", buildCmd);

        running_.store(true);
        reloadPending_.store(false);

        std::thread(&ModuleBuildHelper::Run, this,
                    buildDir, vcvarsall, buildCmd).detach();
        return true;
    }

private:
    std::atomic<bool> running_       {false};
    std::atomic<bool> reloadPending_ {false};

    void Run(std::string buildDir, std::string vcvarsall, std::string buildCmd)
    {
        namespace fs = std::filesystem;

#ifdef _WIN32
        const fs::path batFile = fs::path(buildDir) / "_80cc_module_build.bat";
        {
            std::ofstream bat(batFile);
            bat << "@echo off\r\n";
            if (!vcvarsall.empty())
                bat << "call \"" << vcvarsall << "\" x64 > nul 2>&1\r\n";
            bat << buildCmd << "\r\n";
            bat << "exit /b %errorlevel%\r\n";
        }
        FILE* pipe = _popen(("\"" + batFile.string() + "\" 2>&1").c_str(), "r");
#else
        FILE* pipe = popen((buildCmd + " 2>&1").c_str(), "r");
#endif

        if (!pipe)
        {
            spdlog::error("[ModuleBuild] Failed to launch build process");
            running_.store(false);
            return;
        }

        char lineBuf[512];
        while (fgets(lineBuf, sizeof(lineBuf), pipe))
        {
            std::string line(lineBuf);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            spdlog::info("[ModuleBuild] {}", line);
        }

#ifdef _WIN32
        int rc = _pclose(pipe);
        { std::error_code ec; fs::remove(batFile, ec); }
#else
        int rc = pclose(pipe);
#endif

        if (rc == 0)
        {
            spdlog::info("[ModuleBuild] Build succeeded — pending reload");
            reloadPending_.store(true);
        }
        else
            spdlog::error("[ModuleBuild] Build FAILED (exit code {})", rc);

        running_.store(false);
    }
};

} // namespace ettycc::build
