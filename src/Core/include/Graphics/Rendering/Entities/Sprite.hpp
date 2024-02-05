#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP

#include <GL/glew.h>
#include <GL/gl.h>

#include "../../Shading/ShaderPipeline.hpp"
#include "../Renderable.hpp"
#include <memory>

namespace ettycc
{
    class Sprite : public Renderable
    {
    private:
        GLuint VAO, VBO, EBO, TEXTURE;
        const char * spriteFilePath_;

    public:
        ShaderPipeline underlyingShader;
    
    public:
        Sprite(const char * spriteFilePath);
        ~Sprite();

        void LoadShaders();
        void LoadTextures();

    // Renderable
    public:
        void Pass(const std::shared_ptr<RenderingContext>& ctx, float time) override;
    };

} // namespace ettycc


#endif