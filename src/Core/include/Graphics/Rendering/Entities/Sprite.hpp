#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP

#include <GL/glew.h>
#include <GL/gl.h>

#include "../../Shading/ShaderPipeline.hpp"
#include "../Renderable.hpp"
#include <memory>
#include <string>

#include <cereal/archives/json.hpp>

namespace ettycc
{
    class Sprite : public Renderable
    {
    private:
        GLuint VAO, VBO, EBO, TEXTURE;
        std::string spriteFilePath_;

    public:
        ShaderPipeline underlyingShader;
    
    public:
        Sprite();
        Sprite(const std::string& spriteFilePath, bool initialize = true);
        ~Sprite();

        void Init();
        void LoadShaders();
        void LoadTextures();

    // Renderable
    public:
        void Pass(const std::shared_ptr<RenderingContext>& ctx, float time) override;

        // Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(spriteFilePath_));
        }
    };

} // namespace ettycc


#endif