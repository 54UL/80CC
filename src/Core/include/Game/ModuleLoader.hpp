#pragma once
#include <Game/ModuleLibrary.hpp>
#include <Game/GameModule.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

struct ImGuiContext;  // forward-decl matching imgui.h (global scope)

namespace ettycc
{

class Engine;

// Function pointer types matching the ETTYCC_MODULE() exports.
using CreateModuleFn       = GameModule*(*)();
using DestroyModuleFn      = void(*)(GameModule*);
using SetImGuiContextFn    = void(*)(::ImGuiContext*);

// One loaded DLL module and its runtime state.
struct LoadedModule
{
    std::string                     sourcePath;    // original DLL on disk
    std::string                     loadedPath;    // temp copy that is actually loaded
    ModuleLibrary                   library;
    GameModule*                     instance   = nullptr;
    DestroyModuleFn                 destroyFn  = nullptr;
    std::filesystem::file_time_type lastWriteTime {};

    // -- Metadata -------------------------------------------------------------
    int                             reloadCount = 0;     // how many hot-reloads
    std::chrono::steady_clock::time_point firstLoadTime {};
    std::chrono::steady_clock::time_point lastReloadTime {};
};

// -- ModuleLoader -------------------------------------------------------------
// Loads GameModule DLLs at runtime with hot-reload support.
//
// Design decisions:
//   * Copy-on-load -- the compiler may lock the original DLL while writing the
//     PDB, so we copy it to a timestamped temp name before LoadLibrary.
//   * Old DLLs are unloaded -- after OnDestroy, empty ECS pools are purged
//     so no vtables from the old DLL remain in the Registry.  The old library
//     handle is freed and the temp file deleted.
//   * File-timestamp polling -- simple, portable, no platform-specific
//     watchers.  Checked once per second via PollForReloads().
class ModuleLoader
{
public:
    ModuleLoader()  = default;
    ~ModuleLoader();

    // Load a module DLL.  Copies to a temp name, loads, calls CreateModule
    // and GameModule::OnStart.
    bool LoadModule(const std::string& dllPath, Engine* engine);

    // Scan a directory for *.dll / *.so files and load each one.
    // Also cleans up stale _hot_ files from previous sessions.
    int  LoadModulesFromDirectory(const std::string& dirPath, Engine* engine);

    // Check every loaded module for file changes; hot-reload if needed.
    // Call once per frame (internally throttled by reloadCheckInterval).
    void PollForReloads(Engine* engine, float deltaTime);

    // Force-reload every loaded module (regardless of timestamps).
    // Typically called after a manual recompile triggered from the editor.
    void ForceReloadAll(Engine* engine);

    // All active module instances (for per-frame OnUpdate calls).
    std::vector<GameModule*> GetModules() const;

    // Original DLL source paths for every loaded module (used by ModuleBuildHelper
    // to derive cmake target names).
    std::vector<std::string> GetModuleSourcePaths() const;

    // Read-only access to loaded module metadata (for debug UI).
    const std::vector<LoadedModule>& GetLoadedModules() const { return modules_; }

    float reloadCheckInterval = 1.0f; // seconds between file-stat checks

private:
    std::string CopyToTemp(const std::string& sourcePath);
    bool        ReloadModule(size_t index, Engine* engine);

    // Remove stale _hot_ temp DLL files from a directory.
    static void CleanupStaleHotFiles(const std::string& dirPath);

    std::vector<LoadedModule> modules_;
    float                     timeSinceLastCheck_ = 0.f;
};

} // namespace ettycc
