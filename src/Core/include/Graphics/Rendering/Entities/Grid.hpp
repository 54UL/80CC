#ifndef RENDERING_GRID_HPP
#define RENDERING_GRID_HPP

#include <Graphics/Shading/ShaderPipeline.hpp>
#include <Graphics/Rendering/Renderable.hpp>
#include <GL/glew.h>
#include <GL/gl.h>
#include <string>

namespace ettycc
{
    class Grid : public Renderable
    {
    public:
        Grid();
        ~Grid() override;

        void Init(const std::shared_ptr<Engine>& engineCtx) override;
        void Pass(const std::shared_ptr<RenderingContext>& ctx, float deltaTime) override;

    private:
        GLuint VAO_ = 0;
        GLuint VBO_ = 0;

        ShaderPipeline shader_;

        // cached uniform locations
        GLint pvmLoc_    = -1;
        GLint modelLoc_  = -1;
        GLint camPosLoc_ = -1;

        void        BuildGeometry();
        std::string LoadShaderFile(const std::string& path);
    };
} // namespace ettycc

#endif // RENDERING_GRID_HPP
