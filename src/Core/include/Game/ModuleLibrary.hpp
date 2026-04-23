#pragma once
#include <string>
#include <spdlog/spdlog.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace ettycc
{

// Platform abstraction for loading a shared library and resolving symbols.
// Intentionally does NOT unload in the destructor — hot-reload keeps old
// libraries alive to avoid dangling vtables from template instantiations.
class ModuleLibrary
{
public:
    ModuleLibrary() = default;
    ~ModuleLibrary() = default;

    ModuleLibrary(const ModuleLibrary&)            = delete;
    ModuleLibrary& operator=(const ModuleLibrary&) = delete;

    ModuleLibrary(ModuleLibrary&& o) noexcept
        : handle_(o.handle_), path_(std::move(o.path_))
    { o.handle_ = nullptr; }

    ModuleLibrary& operator=(ModuleLibrary&& o) noexcept
    {
        handle_ = o.handle_;
        path_   = std::move(o.path_);
        o.handle_ = nullptr;
        return *this;
    }

    bool Load(const std::string& path)
    {
        path_ = path;
#ifdef _WIN32
        handle_ = static_cast<void*>(LoadLibraryA(path.c_str()));
#else
        handle_ = dlopen(path.c_str(), RTLD_NOW);
#endif
        if (!handle_)
        {
#ifdef _WIN32
            spdlog::error("[ModuleLibrary] LoadLibrary failed for '{}': error {}",
                          path, GetLastError());
#else
            spdlog::error("[ModuleLibrary] dlopen failed for '{}': {}",
                          path, dlerror());
#endif
            return false;
        }
        spdlog::info("[ModuleLibrary] Loaded '{}'", path);
        return true;
    }

    template<typename T>
    T GetSymbol(const char* name) const
    {
        if (!handle_) return nullptr;
#ifdef _WIN32
        return reinterpret_cast<T>(
            GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
        return reinterpret_cast<T>(dlsym(handle_, name));
#endif
    }

    bool IsLoaded() const { return handle_ != nullptr; }
    const std::string& GetPath() const { return path_; }

private:
    void*       handle_ = nullptr;
    std::string path_;
};

} // namespace ettycc
