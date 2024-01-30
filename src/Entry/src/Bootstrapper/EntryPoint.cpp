#include <80CC.hpp>
#include <Bootstrapper/EntryPoint.hpp>

#include <memory>
#include <iostream>

int main(int argc, char* argv[]) 
{
    std::unique_ptr<ettycc::App> app = std::make_unique<ettycc::SDL2App>();

    if (app->Init(argc, argv))
        return 1;

    return app->Exec();
}