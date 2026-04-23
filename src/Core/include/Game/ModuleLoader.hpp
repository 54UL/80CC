#pragma once
#include <Game/ModuleLibrary.hpp>
#include <Game/GameModule.hpp>

#include <string>
#include <vector>
#include <filesystem>

namespace ettycc
{

class Engine;

// Function pointer types matching the ETTYCC_MODULE() exports.
using CreateModuleFn  = GameModule*(*)();
using DestroyModuleFn = void(*)(GameModule*);

// One loaded DLL module and its runtime state.
struct LoadedModule
{
    std::string                     sourcePath;    // original DLL on disk
    std::string                     loadedPath;    // temp copy that is actually loaded
    ModuleLibrary                   library;
    GameModule*                     instance   = nullptr;
    DestroyModuleFn                 destroyFn  = nullptr;
    std::filesystem::file_time_type lastWriteTime {};
};

// ── ModuleLoader ─────────────────────────────────────────────────────────────
// Loads GameModule DLLs at runtime with hot-reload support.
//
// Design decisions:
//   * Copy-on-load — the compiler may lock the original DLL while writing the
//     PDB, so we copy it to a timestamped temp name before LoadLibrary.
//   * Old DLLs stay loaded — template instantiations (ComponentPool<T>,
//     Holder<T>) create vtables inside the DLL.  Unloading would leave
//     dangling pointers.  The leak is bounded to dev sessions; a restart
//     cleans it up.
//   * File-timestamp polling — simple, portable, no platform-specific
//     watchers.  Checked once per second via CheckForReloads().
class ModuleLoader
{
public:
    ModuleLoader()  = default;
    ~ModuleLoader();

    // Load a module DLL.  Copies to a temp name, loads, calls CreateModule
    // and GameModule::OnStart.
    bool LoadModule(const std::string& dllPath, Engine* engine);

    // Scan a directory for *.dll / *.so files and load each one.
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

    float reloadCheckInterval = 1.0f; // seconds between file-stat checks

private:
    std::string CopyToTemp(const std::string& sourcePath);
    bool        ReloadModule(size_t index, Engine* engine);

    std::vector<LoadedModule>  modules_;
    std::vector<ModuleLibrary> oldLibraries_; // keeps old DLLs alive
    float                      timeSinceLastCheck_ = 0.f;
};

} // namespace ettycc
