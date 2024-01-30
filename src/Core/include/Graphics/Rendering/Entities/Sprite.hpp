#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP
#include "../../Shading/ShaderPipeline.hpp"
#include "../Renderable.hpp"

namespace ettycc
{
    class Sprite : public Renderable
    {
    private:
        GLuint VAO, VBO, EBO, TEXTURE;

    public:
        ShaderPipeline underlyingShader;
    
    public:
        Sprite();
        ~Sprite();

        void LoadShaders();
        void LoadTextures();

    // Renderable
    public:
        void Pass(const std::shared_ptr<RenderingContext>& ctx) override;
    };

} // namespace ettycc


#endif