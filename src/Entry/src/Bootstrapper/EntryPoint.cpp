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

#ifndef COMPILE_80CC_STAND_ALONE_EXECUTABLE
    auto developmentEditor = std::make_shared<DevEditor>();
    app->AddExecutionPipeline(developmentEditor);
#endif

    // Dependency registration (move this!!)
    RegisterDependency(Engine, engineInstance);
    RegisterDependency(Resources, resourcesInstance);
    
    // IMPORTANT: THIS TAG BELOW IS WHAT IT DOES THE COMPILATION MAGIC SO DON'T FUCKING REMOVE THX
    //_80CC_USER_CODE

    if (app->Init(argc, argv))
        return 1;

    return app->Exec();
}