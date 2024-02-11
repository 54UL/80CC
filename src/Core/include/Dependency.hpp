#ifndef DEPENDECY_HPP
#define DEPENDECY_HPP

#include <Engine.hpp>
#include <memory>

// BRIEF:
// SINGLETONS AND ALL SHARED STATIC DEPENDENCIES ACROSS ALL THE WHOLE CORE(I SAID)

namespace ettycc
{  
    namespace EngineSingleton
    {
        // TODO: ADD INSTANCE COUNT CHECK TO AVOID SETTING DIFERENT INSTANCES OR EVEN ASSERT IF NO INSTANCE SETTED...
        static std::shared_ptr<Engine> engine_g;
    }
} // namespace ettycc


#endif