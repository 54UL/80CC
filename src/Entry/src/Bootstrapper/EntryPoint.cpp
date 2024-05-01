#include <80CC.hpp>
#include <Bootstrapper/EntryPoint.hpp>

#include <memory>
#include <iostream>

using namespace ettycc; // IMPORTANT!!!

int main(int argc, char* argv[]) 
{
    std::shared_ptr<App> app = std::make_shared<SDL2App>("80CC");
    
    std::shared_ptr<Engine> engineInstance = std::make_shared<Engine>(app);
    std::shared_ptr<Resources> resourcesInstance = std::make_shared<Resources>();

    auto developmentEditor = std::make_shared<DevEditor>();

    // Dependency registration (move this!!)
    RegisterDependency(Engine, engineInstance);
    RegisterDependency(Resources, resourcesInstance);

    // Configure the engine instance and the development UI (IF NEEDED...)
    app->AddExecutionPipeline(developmentEditor);

    if (app->Init(argc, argv))
        return 1;

    return app->Exec();
}