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
    }

    void RenderableNode::OnUpdate(float deltaTime)
    {
        // Process updates on the renderable if changed...???
        // REQUEST HIDE IF DISABLED???
        // if deleted remove from the rendering...???
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
        //     → Sprite::Inspect        (PROP_SECTION "Sprite" + texture path)
        //       → Renderable::Inspect  (enabled + PROP_SECTION "Transform")
        //         → Transform::Inspect (position / rotation / scale)
        renderable_->Inspect(v);

        PROP_RO(renderableId_, "Renderable ID");
    }
}
