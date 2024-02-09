#ifndef RENDERABLE_NODE_HPP
#define RENDERABLE_NODE_HPP

#include <Engine.hpp>
#include <Scene/NodeComponent.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>

#include <memory>

namespace ettycc
{   
    // Front-end class for the renderable interface...
    class RenderableNode : public NodeComponent
    {
        const char * COLLOQUIAL_NAME = "Renderable";
        
    private:
        std::shared_ptr<Renderable> renderable_;

    public:
        RenderableNode(const std::shared_ptr<Renderable>& renderable);
        ~RenderableNode();

        // NodeComponent api...
        NodeComponentInfo GetComponentInfo() override;
        void OnStart(Engine *engineInstance) override;
        void OnUpdate(float deltaTime) override;
    };  
} // namespace ettycc


#endif