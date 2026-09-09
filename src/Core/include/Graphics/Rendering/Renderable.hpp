#ifndef IRENDERABLE_HPP
#define IRENDERABLE_HPP
#include "RenderingContext.hpp"

#include <memory>
#include <cstdint>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <Scene/Transform.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    class Engine;
    struct EditorPropertyVisitor; // forward -- keeps imgui out of this header

    class Renderable
    {
    public:
        // Fast type tag -- avoids dynamic_cast in hot render loops.
        enum class Type : uint8_t { Unknown, Sprite, Camera, Grid, SoftBody };
        Type renderableType = Type::Unknown;

        // Fallback transform used for serialization and when no entity binding
        // exists.  Prefer transform() for runtime reads.
        Transform underylingTransform;
        bool enabled;
        bool initialized;
        bool initializable_;

        // Render layer bitmask -- bit N = layer N.  Default = layer 0 ("Default").
        // Cameras use cullingMask to filter which layers they render.
        uint32_t layer = 1; // bit 0 = Default layer

    private:
        // Points to the entity's transform when bound, otherwise to
        // underylingTransform.  This is the single source of truth at runtime.
        Transform* transformSource_ = &underylingTransform;

    public:
        Renderable() : initializable_(true) {
            enabled = true;
            initialized = false;
        }

        virtual ~Renderable()= default;

        // Bind to an external transform (e.g. SceneNode::transform_).
        // After binding, transform() returns the entity's transform directly
        // -- no per-frame copy needed.
        void BindTransform(Transform* t) { transformSource_ = t ? t : &underylingTransform; }

        // Runtime accessor -- returns the bound transform or the local fallback.
        Transform&       transform()       { return *transformSource_; }
        const Transform& transform() const { return *transformSource_; }

        virtual void SetTransform(const Transform &trans)
        {
            *transformSource_ = trans;
        }

        virtual Transform GetTransform()
        {
            return *transformSource_;
        }

        virtual void Init(const std::shared_ptr<Engine>& engineCtx) = 0;
        virtual void Pass(const std::shared_ptr<RenderingContext> &ctx, float deltaTime) = 0;

        // Override in subclasses to expose type-specific fields.
        // Default shows the base (enabled + transform).
        // Implemented in each subclass .cpp -- keeps imgui out of this header.
        virtual void Inspect(EditorPropertyVisitor& v);
        // Override to participate in object picking. Default is no-op (cameras, grid, etc.)
        virtual void DrawForPicker(const std::shared_ptr<RenderingContext>& /*ctx*/,
                                   GLuint /*program*/, uint32_t /*id*/) {}

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(underylingTransform), CEREAL_NVP(enabled), CEREAL_NVP(layer));
        }
    };

} // namespace ettycc

#endif