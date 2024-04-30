#ifndef RENDERABLE_NODE_HPP
#define RENDERABLE_NODE_HPP

#include <Engine.hpp>
#include <Scene/NodeComponent.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <memory>

#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{   
    // Front-end class for the renderable interface...
    // Implements sprite, camera and all rendering stuff on the node system

    class RenderableNode : public NodeComponent
    {
        const char * COLLOQUIAL_NAME = "Renderable";
    
    private:
        std::shared_ptr<Renderable> renderable_;

    public:
        RenderableNode(const std::shared_ptr<Renderable>& renderable);
        ~RenderableNode();

        // NodeComponent interface...
        NodeComponentInfo GetComponentInfo() override;
        void OnStart(std::shared_ptr<Engine> engineInstance) override;
        void OnUpdate(float deltaTime) override;

    public:
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(renderable_));
        }
    };  
} // namespace ettycc


#endif