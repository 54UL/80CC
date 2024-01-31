#include <80CC.hpp>
#include <Bootstrapper/EntryPoint.hpp>

#include <memory>
#include <iostream>

int main(int argc, char* argv[]) 
{
    std::shared_ptr<ettycc::App> app = std::make_shared<ettycc::SDL2App>("80CC");
    auto instancedEngine = std::make_shared<ettycc::Engine>(app);
    auto developmentEditor = std::make_shared<ettycc::DevEditor>(instancedEngine);

    // Configure the engine instance and the development UI (IF NEEDED...)
    app->SetUnderlyingEngine(instancedEngine);
    app->AddExecutionPipeline(developmentEditor);

    if (app->Init(argc, argv))
        return 1;

    return app->Exec();
}