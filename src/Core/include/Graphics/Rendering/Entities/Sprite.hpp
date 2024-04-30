#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP

#include <GL/glew.h>
#include <GL/gl.h>

#include "../../Shading/ShaderPipeline.hpp"
#include "../Renderable.hpp"
#include <memory>

#include <cereal/archives/json.hpp>

namespace ettycc
{
    class Sprite : public Renderable
    {
    private:
        GLuint VAO, VBO, EBO, TEXTURE;
        const char * spriteFilePath_;
        std::string spriteFilePathStr_;

    public:
        ShaderPipeline underlyingShader;
    
    public:
        Sprite();
        Sprite(const char * spriteFilePath);
        ~Sprite();

        void LoadShaders();
        void LoadTextures();

    // Renderable
    public:
        void Pass(const std::shared_ptr<RenderingContext>& ctx, float time) override;

        // Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(spriteFilePathStr_));
        }
    };

} // namespace ettycc


#endif