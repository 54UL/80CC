#ifndef IGAME_MODULE_HPP
#define IGAME_MODULE_HPP

#include <memory>
#include <string>

namespace ettycc
{
    class Engine;
    class GameModule
    {
    public:
        GameModule() = default;
        virtual ~GameModule() = default;

        std::string name_;// todo: not shure if is this right...
        virtual bool OnStart(const Engine* engine) = 0;
        virtual void OnUpdate(const float deltaTime) = 0;
        virtual void OnDestroy() = 0;     
    };    
} // namespace ettycc

#endif