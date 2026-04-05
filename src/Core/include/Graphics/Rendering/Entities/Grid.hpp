#ifndef RENDERING_GRID_HPP
#define RENDERING_GRID_HPP

#include <Graphics/Rendering/Renderable.hpp>
#include <Scene/Assets/ResourceCache.hpp>
#include <GL/glew.h>
#include <GL/gl.h>
#include <string>
#include <memory>

namespace ettycc
{
    class Grid : public Renderable
    {
        static constexpr const char* kShaderName = "grid";

    public:
        Grid();
        ~Grid() override;

        void Init(const std::shared_ptr<Engine>& engineCtx) override;
        void Pass(const std::shared_ptr<RenderingContext>& ctx, float deltaTime) override;

    private:
        GLuint VAO_ = 0;
        GLuint VBO_ = 0;

        std::shared_ptr<CachedShader> cachedShader_;

        // cached uniform locations
        GLint pvmLoc_      = -1;
        GLint modelLoc_    = -1;
        GLint camPosLoc_   = -1;
        GLint viewSizeLoc_ = -1;

        void BuildGeometry();
    };
} // namespace ettycc

#endif // RENDERING_GRID_HPP
