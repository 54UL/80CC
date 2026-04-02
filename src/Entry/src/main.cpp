#include <80CC.hpp>
#include <main.hpp>

#include <memory>
#include <iostream>

_80CC_USER_INCLUDES;

using namespace ettycc; // IMPORTANT!!!

int main(int argc, char* argv[]) 
{
    std::shared_ptr<App> app = std::make_shared<SDL2App>(COMPILED_EXEC_NAME);
    std::shared_ptr<Engine> engineInstance = std::make_shared<Engine>(app);
    std::shared_ptr<Globals> resourcesInstance = std::make_shared<Globals>();

    _80CC_ASSET_INIT;

#ifndef COMPILE_80CC_STAND_ALONE_EXECUTABLE
    // Editor build: tell the engine so Init() restores the last scene instead of running modules.
    engineInstance->SetEditorMode(true);
    auto developmentEditor = std::make_shared<DevEditor>(engineInstance);
    app->AddExecutionPipeline(developmentEditor);
#endif

    // Dependency registration (move this!!)
    RegisterDependency(Engine, engineInstance);
    RegisterDependency(Globals, resourcesInstance);

    if (app->Init(argc, argv))
        return 1;

    _80CC_USER_CODE;

    int exitCode = app->Exec();

    // Release all dependency-injected shared_ptrs before the static Dependency
    // singleton is torn down.  Without this, Engine/Globals destructors would
    // fire in the static cleanup phase — after SDL2App and spdlog statics are
    // potentially already gone — causing crashes in their destructors.
    Dependency::getInstance().Clear();

    return exitCode;
}