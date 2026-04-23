#ifndef IGAME_MODULE_HPP
#define IGAME_MODULE_HPP

#include <memory>
#include <string>

// ── DLL export / import macros ───────────────────────────────────────────────
// Module DLLs define ETTYCC_MODULE_EXPORT before including this header
// (set automatically by the CMake target_compile_definitions).
#ifdef _WIN32
    #ifdef ETTYCC_MODULE_EXPORT
        #define ETTYCC_MODULE_API __declspec(dllexport)
    #else
        #define ETTYCC_MODULE_API __declspec(dllimport)
    #endif
#else
    #define ETTYCC_MODULE_API __attribute__((visibility("default")))
#endif

namespace ettycc
{
    class Engine;
    class GameModule
    {
    public:
        GameModule() = default;
        virtual ~GameModule() = default;

        std::string name_;
        virtual bool OnStart(const Engine* engine) = 0;
        virtual void OnUpdate(const float deltaTime) = 0;
        virtual void OnDestroy() = 0;
    };
} // namespace ettycc

// ── DLL entry point macro ────────────────────────────────────────────────────
// Place ETTYCC_MODULE(YourModuleClass) in exactly one .cpp file per module DLL.
// It exports the C functions that ModuleLoader uses to create/destroy instances.
#define ETTYCC_MODULE(ClassName)                                              \
    extern "C" ETTYCC_MODULE_API ettycc::GameModule* ettycc_CreateModule() {  \
        return new ClassName();                                               \
    }                                                                         \
    extern "C" ETTYCC_MODULE_API void ettycc_DestroyModule(                   \
            ettycc::GameModule* m) {                                          \
        delete m;                                                             \
    }

#endif
