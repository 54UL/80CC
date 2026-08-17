#include <Game/ModuleLoader.hpp>
#include <Engine.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

namespace ettycc
{

// -- Lifecycle ----------------------------------------------------------------

ModuleLoader::~ModuleLoader()
{
    for (auto& mod : modules_)
    {
        if (mod.instance)
        {
            mod.instance->OnDestroy();
            if (mod.destroyFn)
                mod.destroyFn(mod.instance);
            mod.instance = nullptr;
        }
        // Unload the library and delete the temp file.
        mod.library.Unload();
        {
            std::error_code ec;
            fs::remove(mod.loadedPath, ec);
        }
    }
}

// -- Stale file cleanup -------------------------------------------------------

void ModuleLoader::CleanupStaleHotFiles(const std::string& dirPath)
{
    std::error_code ec;
    if (!fs::is_directory(dirPath, ec)) return;

    int removed = 0;
    for (const auto& entry : fs::directory_iterator(dirPath, ec))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().stem().string().find("_hot_") != std::string::npos)
        {
            fs::remove(entry.path(), ec);
            if (!ec) ++removed;
        }
    }
    if (removed > 0)
        spdlog::info("[ModuleLoader] Cleaned up {} stale _hot_ file(s) from '{}'",
                     removed, dirPath);
}

// -- Copy-on-load -------------------------------------------------------------

std::string ModuleLoader::CopyToTemp(const std::string& sourcePath)
{
    const auto source  = fs::path(sourcePath);
    const auto stem    = source.stem().string();
    const auto ext     = source.extension().string();
    const auto dir     = source.parent_path();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();

    auto tempPath = dir / (stem + "_hot_" + std::to_string(ms) + ext);

    std::error_code ec;
    fs::copy_file(sourcePath, tempPath, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        spdlog::error("[ModuleLoader] copy_file '{}' -> '{}' failed: {}",
                      sourcePath, tempPath.string(), ec.message());
        return {};
    }
    return tempPath.string();
}

// -- Load ---------------------------------------------------------------------

bool ModuleLoader::LoadModule(const std::string& dllPath, Engine* engine)
{
    std::error_code ec;
    if (!fs::exists(dllPath, ec))
    {
        spdlog::error("[ModuleLoader] DLL not found: '{}'", dllPath);
        return false;
    }

    std::string loadedPath = CopyToTemp(dllPath);
    if (loadedPath.empty()) return false;

    LoadedModule mod;
    mod.sourcePath = fs::canonical(dllPath, ec).string();
    mod.loadedPath = loadedPath;

    if (!mod.library.Load(loadedPath))
        return false;

    auto createFn  = mod.library.GetSymbol<CreateModuleFn>("ettycc_CreateModule");
    mod.destroyFn  = mod.library.GetSymbol<DestroyModuleFn>("ettycc_DestroyModule");

    if (!createFn || !mod.destroyFn)
    {
        spdlog::error("[ModuleLoader] '{}' missing ettycc_CreateModule / "
                      "ettycc_DestroyModule exports", dllPath);
        mod.library.Unload();
        { std::error_code ec2; fs::remove(loadedPath, ec2); }
        return false;
    }

    // Share the engine's ImGui context with the DLL so that module code
    // calling ImGui (e.g. PROP macro in InspectProperties) uses the
    // correct context instead of the DLL's own uninitialized GImGui.
    auto setCtxFn = mod.library.GetSymbol<SetImGuiContextFn>("ettycc_SetImGuiContext");
    if (setCtxFn)
        setCtxFn(ImGui::GetCurrentContext());

    mod.instance = createFn();
    if (!mod.instance)
    {
        spdlog::error("[ModuleLoader] ettycc_CreateModule returned null for '{}'",
                      dllPath);
        mod.library.Unload();
        { std::error_code ec2; fs::remove(loadedPath, ec2); }
        return false;
    }

    mod.lastWriteTime  = fs::last_write_time(mod.sourcePath, ec);
    mod.firstLoadTime  = std::chrono::steady_clock::now();
    mod.lastReloadTime = mod.firstLoadTime;
    mod.reloadCount    = 0;

    mod.instance->OnStart(engine);
    spdlog::info("[ModuleLoader] Module '{}' loaded from '{}'",
                 mod.instance->name_, dllPath);

    modules_.push_back(std::move(mod));
    return true;
}

// -- Directory scan -----------------------------------------------------------

int ModuleLoader::LoadModulesFromDirectory(const std::string& dirPath,
                                           Engine* engine)
{
    std::error_code ec;
    if (!fs::is_directory(dirPath, ec))
    {
        spdlog::warn("[ModuleLoader] modules directory '{}' not found -- skipping",
                     dirPath);
        return 0;
    }

    // Remove stale _hot_ DLLs left by a previous session.
    CleanupStaleHotFiles(dirPath);

    int loaded = 0;
    for (const auto& entry : fs::directory_iterator(dirPath, ec))
    {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();

        // Skip hot-reload temp copies (shouldn't exist after cleanup, but guard)
        if (entry.path().stem().string().find("_hot_") != std::string::npos)
            continue;

#ifdef _WIN32
        if (ext != ".dll") continue;
#else
        if (ext != ".so") continue;
#endif
        if (LoadModule(entry.path().string(), engine))
            ++loaded;
    }

    spdlog::info("[ModuleLoader] Loaded {} module(s) from '{}'", loaded, dirPath);
    return loaded;
}

// -- Hot-reload polling -------------------------------------------------------

void ModuleLoader::PollForReloads(Engine* engine, float deltaTime)
{
    timeSinceLastCheck_ += deltaTime;
    if (timeSinceLastCheck_ < reloadCheckInterval)
        return;
    timeSinceLastCheck_ = 0.f;

    for (size_t i = 0; i < modules_.size(); ++i)
    {
        auto& mod = modules_[i];
        std::error_code ec;
        if (!fs::exists(mod.sourcePath, ec)) continue;

        auto currentTime = fs::last_write_time(mod.sourcePath, ec);
        if (ec || currentTime == mod.lastWriteTime) continue;

        // File changed -- give the compiler a moment to finish writing
        // (linker may still be flushing).  A short sleep avoids partial reads.
#ifdef _WIN32
        Sleep(200);
#else
        usleep(200000);
#endif
        ReloadModule(i, engine);
    }
}

// -- Reload -------------------------------------------------------------------

bool ModuleLoader::ReloadModule(size_t index, Engine* engine)
{
    auto& mod = modules_[index];
    const std::string oldName = mod.instance ? mod.instance->name_ : "<unknown>";

    spdlog::info("[ModuleLoader] Hot-reloading '{}' (reload #{})",
                 oldName, mod.reloadCount + 1);

    // -- 1. Tear down old instance --------------------------------------------
    if (mod.instance)
    {
        mod.instance->OnDestroy();
        if (mod.destroyFn)
            mod.destroyFn(mod.instance);
        mod.instance = nullptr;
    }

    // -- 2. Purge empty pools so no vtables from the old DLL remain -----------
    if (engine->mainScene_)
        engine->mainScene_->registry_.PurgeEmptyPools();

    // -- 3. Unload old library and delete old temp file -----------------------
    const std::string oldTempPath = mod.loadedPath;
    mod.library.Unload();
    {
        std::error_code ec;
        fs::remove(oldTempPath, ec);
        if (ec)
            spdlog::warn("[ModuleLoader] Could not delete old temp '{}': {}",
                         oldTempPath, ec.message());
    }

    // -- 4. Load new copy -----------------------------------------------------
    std::string newLoadedPath = CopyToTemp(mod.sourcePath);
    if (newLoadedPath.empty()) return false;

    mod.loadedPath = newLoadedPath;
    if (!mod.library.Load(newLoadedPath)) return false;

    auto createFn = mod.library.GetSymbol<CreateModuleFn>("ettycc_CreateModule");
    mod.destroyFn = mod.library.GetSymbol<DestroyModuleFn>("ettycc_DestroyModule");
    if (!createFn || !mod.destroyFn)
    {
        spdlog::error("[ModuleLoader] Reloaded DLL missing exports");
        return false;
    }

    // Re-share ImGui context with the freshly loaded DLL.
    auto setCtxFn = mod.library.GetSymbol<SetImGuiContextFn>("ettycc_SetImGuiContext");
    if (setCtxFn)
        setCtxFn(ImGui::GetCurrentContext());

    mod.instance = createFn();
    if (!mod.instance) return false;

    std::error_code ec;
    mod.lastWriteTime  = fs::last_write_time(mod.sourcePath, ec);
    mod.lastReloadTime = std::chrono::steady_clock::now();
    mod.reloadCount++;

    mod.instance->OnStart(engine);
    spdlog::info("[ModuleLoader] Hot-reload #{} complete: '{}'",
                 mod.reloadCount, mod.instance->name_);
    return true;
}

// -- Force-reload all -----------------------------------------------------

void ModuleLoader::ForceReloadAll(Engine* engine)
{
    spdlog::info("[ModuleLoader] Force-reloading all {} module(s)...", modules_.size());
    for (size_t i = 0; i < modules_.size(); ++i)
        ReloadModule(i, engine);
}

// -- Query --------------------------------------------------------------------

std::vector<GameModule*> ModuleLoader::GetModules() const
{
    std::vector<GameModule*> result;
    result.reserve(modules_.size());
    for (const auto& mod : modules_)
        if (mod.instance)
            result.push_back(mod.instance);
    return result;
}

std::vector<std::string> ModuleLoader::GetModuleSourcePaths() const
{
    std::vector<std::string> result;
    result.reserve(modules_.size());
    for (const auto& mod : modules_)
        result.push_back(mod.sourcePath);
    return result;
}

} // namespace ettycc
