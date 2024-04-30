#include <Scene/Components/RenderableNode.hpp>

namespace ettycc
{

    RenderableNode::RenderableNode()
    {

    }

    RenderableNode::RenderableNode(const std::shared_ptr<Renderable>& renderable)
    {
        renderable_= renderable;
    }

    RenderableNode::~RenderableNode()
    {

    }

    NodeComponentInfo RenderableNode::GetComponentInfo()
    {
        return NodeComponentInfo {0, COLLOQUIAL_NAME, true, ProcessingChannel::RENDERING};
    }

    void RenderableNode::OnStart(std::shared_ptr<Engine> engineInstance)
    {
        engineInstance->renderEngine_.AddRenderable(renderable_);
    }

    void RenderableNode::OnUpdate(float deltaTime)
    {
        // Process updates on the renderable if changed...???
        // REQUEST HIDE IF DISABLED???
        // if deleted remove from the rendering...???
    }
}
