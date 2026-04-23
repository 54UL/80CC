#include <Game/ModuleLoader.hpp>
#include <Engine.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

namespace ettycc
{

// ── Lifecycle ────────────────────────────────────────────────────────────────

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
    }
    // oldLibraries_ and modules_ destructors don't FreeLibrary — intentional.
}

// ── Copy-on-load ─────────────────────────────────────────────────────────────

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

// ── Load ─────────────────────────────────────────────────────────────────────

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
        return false;
    }

    mod.instance = createFn();
    if (!mod.instance)
    {
        spdlog::error("[ModuleLoader] ettycc_CreateModule returned null for '{}'",
                      dllPath);
        return false;
    }

    mod.lastWriteTime = fs::last_write_time(mod.sourcePath, ec);

    // Let the module initialize — it can register systems, add components, etc.
    mod.instance->OnStart(engine);
    spdlog::info("[ModuleLoader] Module '{}' loaded from '{}'",
                 mod.instance->name_, dllPath);

    modules_.push_back(std::move(mod));
    return true;
}

// ── Directory scan ───────────────────────────────────────────────────────────

int ModuleLoader::LoadModulesFromDirectory(const std::string& dirPath,
                                           Engine* engine)
{
    std::error_code ec;
    if (!fs::is_directory(dirPath, ec))
    {
        spdlog::warn("[ModuleLoader] modules directory '{}' not found — skipping",
                     dirPath);
        return 0;
    }

    int loaded = 0;
    for (const auto& entry : fs::directory_iterator(dirPath, ec))
    {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();

        // Skip hot-reload temp copies from previous sessions
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

// ── Hot-reload polling ───────────────────────────────────────────────────────

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

        // File changed — give the compiler a moment to finish writing
        // (linker may still be flushing).  A short sleep avoids partial reads.
#ifdef _WIN32
        Sleep(200);
#else
        usleep(200000);
#endif
        ReloadModule(i, engine);
    }
}

// ── Reload ───────────────────────────────────────────────────────────────────

bool ModuleLoader::ReloadModule(size_t index, Engine* engine)
{
    auto& mod = modules_[index];
    const std::string oldName = mod.instance ? mod.instance->name_ : "<unknown>";

    spdlog::info("[ModuleLoader] Hot-reloading '{}'...", oldName);

    // ── Tear down old instance ───────────────────────────────────────────────
    if (mod.instance)
    {
        mod.instance->OnDestroy();
        if (mod.destroyFn)
            mod.destroyFn(mod.instance);
        mod.instance = nullptr;
    }

    // Keep old library loaded (template vtables may still be referenced).
    oldLibraries_.push_back(std::move(mod.library));

    // Clean up old temp file (best effort).
    {
        std::error_code ec;
        fs::remove(mod.loadedPath, ec);
    }

    // ── Load new copy ────────────────────────────────────────────────────────
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

    mod.instance = createFn();
    if (!mod.instance) return false;

    std::error_code ec;
    mod.lastWriteTime = fs::last_write_time(mod.sourcePath, ec);

    mod.instance->OnStart(engine);
    spdlog::info("[ModuleLoader] Hot-reload complete: '{}'", mod.instance->name_);
    return true;
}

// ── Force-reload all ─────────────────────────────────────────────────────

void ModuleLoader::ForceReloadAll(Engine* engine)
{
    spdlog::info("[ModuleLoader] Force-reloading all {} module(s)...", modules_.size());
    for (size_t i = 0; i < modules_.size(); ++i)
        ReloadModule(i, engine);
}

// ── Query ────────────────────────────────────────────────────────────────────

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
