#include <80CC.hpp>
#include <main.hpp>

#include <memory>
#include <iostream>

_80CC_USER_INCLUDES;

using namespace ettycc; // IMPORTANT!!!

int main(int argc, char* argv[]) 
{
    _80CC_ASSET_INIT;

    std::shared_ptr<App> app = std::make_shared<SDL2App>("80CC"); // TODO: Use compilation name...

    std::shared_ptr<Engine> engineInstance = std::make_shared<Engine>(app);
    std::shared_ptr<Globals> resourcesInstance = std::make_shared<Globals>();

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
    
    return app->Exec();
}