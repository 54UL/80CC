#ifndef NODE_COMPONENT_HPP
#define NODE_COMPONENT_HPP

// #include <Scene/SceneNode.hpp>
#include <Scene/Api.hpp>

#include <memory>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ettycc 
{   
    class Engine; // forward declaration because engine has included NodeComponent somewhere...

    class NodeComponent
    {
        public:
        virtual NodeComponentInfo GetComponentInfo() = 0;
        virtual void OnStart(std::shared_ptr<Engine> engineInstance) = 0;
        virtual void OnUpdate(float deltaTime) = 0;
    };
}


#endif