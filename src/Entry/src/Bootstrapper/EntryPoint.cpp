#include <80CC.hpp>
#include <Bootstrapper/EntryPoint.hpp>

#include <memory>
#include <iostream>

int main(int argc, char** argv) 
{
    std::unique_ptr<etycc::App> app = std::make_unique<etycc::SDL2App>();

    app->Init(argc, argv);

    return app->Exec();
}