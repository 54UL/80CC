#ifndef RENDERABLE_NODE_HPP
#define RENDERABLE_NODE_HPP

#include <Scene/Api.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <memory>

#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    class Engine;
    struct EditorPropertyVisitor;

    // ── RenderableNode ────────────────────────────────────────────────────────
    // Wraps a Renderable (Sprite, Camera, …) as a data component.
    // Initialization and per-frame sync are performed by RenderSystem.
    class RenderableNode
    {
    public:
        static constexpr const char*        componentType = "Renderable";
        static constexpr ProcessingChannel  channel       = ProcessingChannel::RENDERING;

        RenderableNode() = default;
        explicit RenderableNode(std::shared_ptr<Renderable> renderable);
        ~RenderableNode();

        // Non-copyable, movable — the user-declared destructor suppresses
        // implicit move ops, so we declare them explicitly.
        RenderableNode(const RenderableNode&)            = delete;
        RenderableNode& operator=(const RenderableNode&) = delete;

        RenderableNode(RenderableNode&& o) noexcept
            : renderable_(std::move(o.renderable_))
            , initialized_(o.initialized_)
        {
            o.initialized_ = false;
        }

        RenderableNode& operator=(RenderableNode&& o) noexcept
        {
            if (this == &o) return *this;
            renderable_  = std::move(o.renderable_);
            initialized_ = o.initialized_;
            o.initialized_ = false;
            return *this;
        }

        // ── System-facing API (called by RenderSystem) ────────────────────────
        void InitRenderable(Engine& engine);
        void SyncTransform(const Transform& t);
        bool IsInitialized() const { return initialized_; }

        // ── Editor inspector ──────────────────────────────────────────────────
        void InspectProperties(EditorPropertyVisitor& v);

        // ── Serialization ─────────────────────────────────────────────────────
        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(renderable_));
        }

    public:
        std::shared_ptr<Renderable> renderable_;

    private:
        bool initialized_ = false;
    };

} // namespace ettycc

#endif
