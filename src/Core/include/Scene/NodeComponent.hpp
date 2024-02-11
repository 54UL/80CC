#ifndef NODE_COMPONENT_HPP
#define NODE_COMPONENT_HPP

// #include <Scene/SceneNode.hpp>
#include <Scene/Api.hpp>

#include <memory>
#include <map>
#include <string>
#include <string_view>

namespace ettycc 
{   
    class Engine; // forward declaration because engine has included NodeComponent somewhere...

    class NodeComponent
    {
        public:
        // Life cycle functions
        virtual NodeComponentInfo GetComponentInfo() = 0;
        virtual void OnStart(std::shared_ptr<Engine> engineInstance) = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        // This is more like a serialization interface...
        // virtual void Load();
        // virtual void Store();
    };
}


#endif