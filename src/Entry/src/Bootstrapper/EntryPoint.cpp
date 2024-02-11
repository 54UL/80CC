#include <80CC.hpp>
#include <Bootstrapper/EntryPoint.hpp>

#include <memory>
#include <iostream>

int main(int argc, char* argv[]) 
{
    std::shared_ptr<ettycc::App> app = std::make_shared<ettycc::SDL2App>("80CC");
    ettycc::EngineSingleton::engine_g = std::make_shared<ettycc::Engine>(app);

    auto developmentEditor = std::make_shared<ettycc::DevEditor>();

    // Configure the engine instance and the development UI (IF NEEDED...)
    // app->SetUnderlyingEngine(ettycc::EngineSingleton::engine_g);
    app->AddExecutionPipeline(developmentEditor);

    if (app->Init(argc, argv))
        return 1;

    return app->Exec();
}