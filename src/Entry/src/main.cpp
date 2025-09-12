#include <80CC.hpp>
#include <main.hpp>

#include <memory>
#include <iostream>

// IMPORTANT: THIS TAG BELOW DOES THE COMPILATION MAGIC SO DON'T FUCKING REMOVE THX
//_80CC_USER_INCLUDES

using namespace ettycc; // IMPORTANT!!!

int main(int argc, char* argv[]) 
{
    std::shared_ptr<App> app = std::make_shared<SDL2App>("80CC");
    
    std::shared_ptr<Engine> engineInstance = std::make_shared<Engine>(app);
    std::shared_ptr<Resources> resourcesInstance = std::make_shared<Resources>();

#ifndef COMPILE_80CC_STAND_ALONE_EXECUTABLE
    auto developmentEditor = std::make_shared<DevEditor>(engineInstance);
    app->AddExecutionPipeline(developmentEditor);
#endif

    // Dependency registration (move this!!)
    RegisterDependency(Engine, engineInstance);
    RegisterDependency(Resources, resourcesInstance);
    
    if (app->Init(argc, argv))
        return 1;

    // IMPORTANT: THIS TAG BELOW DOES THE COMPILATION MAGIC SO DON'T FUCKING REMOVE THX
    //_80CC_USER_CODE
    
    return app->Exec();
}