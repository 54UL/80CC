#include <Scene/Components/RenderableNode.hpp>
#include <UI/EditorPropertyVisitor.hpp>

namespace ettycc
{

    RenderableNode::RenderableNode()
    {

    }

    RenderableNode::RenderableNode(const std::shared_ptr<Renderable>& renderable)
    {
        renderable_= renderable;
        renderableId_ = Utils::GetNextIncrementalId();
    }

    RenderableNode::~RenderableNode()
    {

    }

    NodeComponentInfo RenderableNode::GetComponentInfo()
    {
        return NodeComponentInfo {renderableId_, componentType, true, ProcessingChannel::RENDERING};
    }

    void RenderableNode::OnStart(std::shared_ptr<Engine> engineInstance)
    {
        renderable_->Init(engineInstance);
        engineInstance->renderEngine_.AddRenderable(renderable_);
        // Seed the node transform from the renderable (set at creation/deserialization time).
        // Physics bodies override this immediately in RigidBodyComponent::OnStart.
        if (ownerNode_)
            ownerNode_->transform_ = renderable_->underylingTransform;
    }

    void RenderableNode::OnUpdate(float /*deltaTime*/)
    {
        // Keep the renderable in sync with the node's authoritative transform.
        // RigidBodyComponent (MAIN channel) writes physics pos to ownerNode_->transform_
        // before this RENDERING-channel update runs, so rendering always shows the
        // correct position regardless of whether the node is physics-driven or editor-driven.
        if (ownerNode_)
            renderable_->underylingTransform = ownerNode_->transform_;
    }

    void RenderableNode::InspectProperties(EditorPropertyVisitor& v)
    {
        if (!renderable_)
        {
            // Nothing to show but at least surface the ID
            PROP_RO(renderableId_, "Renderable ID");
            return;
        }

        // Delegates to Renderable::Inspect (virtual) so Sprite, Camera, etc.
        // each show their own fields.  The call chain is:
        //   RenderableNode::InspectProperties
        //     -> Sprite::Inspect        (PROP_SECTION "Sprite" + texture path)
        //       -> Renderable::Inspect  (enabled + PROP_SECTION "Transform")
        //         -> Transform::Inspect (position / rotation / scale)
        renderable_->Inspect(v);

        PROP_RO(renderableId_, "Renderable ID");
    }
}
