#ifndef IGAME_MODULE_HPP
#define IGAME_MODULE_HPP

#include <memory>

namespace ettycc
{
    class Engine;
    class GameModule
    {
    public:
        GameModule() = default;
        virtual ~GameModule() = default;

        virtual bool OnStart(std::weak_ptr<Engine> engine) = 0;
        virtual void OnUpdate(const float deltaTime) = 0;
        virtual void OnDestroy() = 0;     
    };    
} // namespace ettycc

#endif