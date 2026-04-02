#include <Scene/Components/RenderableNode.hpp>
#include <Engine.hpp>
#include <UI/EditorPropertyVisitor.hpp>

namespace ettycc
{
    RenderableNode::RenderableNode(std::shared_ptr<Renderable> renderable)
        : renderable_(std::move(renderable))
    {}

    RenderableNode::~RenderableNode() {}

    // ── System-facing API ─────────────────────────────────────────────────────
    void RenderableNode::InitRenderable(Engine& engine)
    {
        if (!renderable_ || initialized_) return;
        renderable_->Init(GetDependency(Engine));
        engine.renderEngine_.AddRenderable(renderable_);
        initialized_ = true;
    }

    void RenderableNode::SyncTransform(const Transform& t)
    {
        if (renderable_)
            renderable_->underylingTransform = t;
    }

    // ── Editor inspector ──────────────────────────────────────────────────────
    void RenderableNode::InspectProperties(EditorPropertyVisitor& v)
    {
        if (!renderable_) return;
        renderable_->Inspect(v);
    }

} // namespace ettycc
