#include <Scene/Components/RenderableNode.hpp>
#include <Engine.hpp>
#include <UI/EditorPropertyVisitor.hpp>

namespace ettycc
{
    RenderableNode::RenderableNode(std::shared_ptr<Renderable> renderable)
        : renderable_(std::move(renderable))
    {}

    RenderableNode::~RenderableNode()
    {
        if (!renderable_) return;

        // Unbind external transform so the renderable falls back to its
        // internal transform -- prevents dangling pointer if the SceneNode
        // (which owns the bound transform) is destroyed before the
        // Renderable's shared_ptr ref count reaches zero.
        renderable_->BindTransform(nullptr);

        // Remove from the render engine so it won't be iterated next frame.
        auto engine = GetDependency(Engine);
        if (engine)
            engine->renderEngine_.RemoveRenderable(renderable_);
    }

    // -- System-facing API -----------------------------------------------------
    void RenderableNode::InitRenderable(Engine& engine)
    {
        if (!renderable_ || initialized_) return;
        renderable_->Init(GetDependency(Engine));
        engine.renderEngine_.AddRenderable(renderable_);
        initialized_ = true;
    }

    void RenderableNode::SyncTransform(Transform& t)
    {
        if (renderable_)
            renderable_->BindTransform(&t);
    }

    // -- Editor inspector ------------------------------------------------------
    void RenderableNode::InspectProperties(EditorPropertyVisitor& v)
    {
        if (!renderable_) return;
        renderable_->Inspect(v);
    }

} // namespace ettycc
