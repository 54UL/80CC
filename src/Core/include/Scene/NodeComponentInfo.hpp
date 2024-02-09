#ifndef NODE_COMPONENT_HPP
#define NODE_COMPONENT_HPP

// #include <Scene/SceneNode.hpp>
// #include <Engine.hpp>
#include <map>
#include <string>
#include <string_view>

namespace ettycc 
{
    class NodeComponent
    {
        public:
        NodeComponent()
        {
            //default values...
            id = 0;
            name = "UNNAMED";
            enabled = true;
            processingChannel = ProcessingChannel::MAIN;
        }

        ~NodeComponent(){}
        
        uint64_t id;
        std::string name;
        bool enabled;
        ProcessingChannel processingChannel;
    };
}


#endif