#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP
#include "../../Shading/ShaderPipeline.hpp"

namespace ettycc
{
    class Sprite
    {
    private:
        GLuint VAO, VBO, EBO, TEXTURE;
        ShaderPipeline UnderlyingShader;

    public:
        Sprite();
        ~Sprite();

        // RENDERING API
        // void Draw();
        void LoadShaders();
        void LoadTextures();
    };

} // namespace ettycc


#endif