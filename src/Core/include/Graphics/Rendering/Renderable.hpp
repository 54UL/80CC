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

    class Renderable
    {
    public:
        Transform underylingTransform;
        bool enabled;
        bool initialized;
        bool initializable_;

    public:
        Renderable() : initializable_(true) {
            enabled = true;
            initialized = false;
        }

        virtual ~Renderable()= default;

        virtual void SetTransform(const Transform &trans)
        {
            underylingTransform = trans;
        }

        virtual Transform GetTransform()
        {
            return underylingTransform;
        }

        virtual void Init(const std::shared_ptr<Engine>& engineCtx) = 0;
        virtual void Pass(const std::shared_ptr<RenderingContext> &ctx, float deltaTime) = 0;
        // Override to participate in object picking. Default is no-op (cameras, grid, etc.)
        virtual void DrawForPicker(const std::shared_ptr<RenderingContext>& /*ctx*/,
                                   GLuint /*program*/, uint32_t /*id*/) {}

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(underylingTransform), CEREAL_NVP(enabled));
        }
    };

} // namespace ettycc

#endif