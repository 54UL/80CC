#ifndef RENDERABLE_NODE_HPP
#define RENDERABLE_NODE_HPP

#include <Scene/NodeComponent.hpp>

namespace ettycc
{   
    // Front-end class for the renderable interface...

    class RenderableNode : public NodeComponent
    {
    private:
        const char* loonaImagePath = "D:/repos2/ALPHA_V1/assets/images/loona.jpg";// TODO: FETCH FROM config???

    public:
        RenderableNode();
        ~RenderableNode();

        void OnStart() override;
        void OnUpdate() override;
    };  
} // namespace ettycc


#endif