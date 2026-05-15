#include <80CC.hpp>
#include <main.hpp>
#include <cpr/cpr.h>
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
    engineInstance->SetEditorMode(true);
    auto developmentEditor = std::make_shared<DevEditor>(engineInstance);
    app->AddExecutionPipeline(developmentEditor);
#endif

    // Dependency registration (move this!!)
    RegisterDependency(Engine, engineInstance);
    RegisterDependency(Globals, resourcesInstance);

    // CPR sample lol
    cpr::Response r = cpr::Get(cpr::Url{"https://api.github.com/repos/libcpr/cpr/contributors"},
                      cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                      cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    r.status_code;                  // 200
    r.header["content-type"];       // application/json; charset=utf-8
    r.text;                         // JSON text string

    if (app->Init(argc, argv))
        return 1;

    _80CC_USER_CODE;

    int exitCode = app->Exec();

    Dependency::getInstance().Clear();

    return exitCode;
}