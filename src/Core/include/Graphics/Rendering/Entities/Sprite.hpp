#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP

#include "../../Shading/ShaderPipeline.hpp"
#include "../Renderable.hpp"

#include <Dependency.hpp>
#include <Dependencies/Resources.hpp>

#include <memory>
#include <string>
#include <fstream>
#include <sstream>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <cereal/archives/json.hpp>

namespace ettycc
{
    class Sprite : public Renderable
    {
        const std::string shaderBaseName_ = "sprite";

    private:
        GLuint VAO, VBO, EBO, TEXTURE;
        std::string spriteFilePath_;
        
    public:
        ShaderPipeline underlyingShader;
    
    public:
        Sprite();
        Sprite(const std::string& spriteFilePath, bool initialize = true);
        ~Sprite();

        void InitBackend();
        void LoadShaders();
        void LoadTextures();

    // Renderable
    public:
        void Init() override;
        void Pass(const std::shared_ptr<RenderingContext>& ctx, float time) override;

        // Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::base_class<Renderable>(this), CEREAL_NVP(spriteFilePath_));
        }
    
    // Internal usage
    private:
        std::string LoadShaderFile(const std::string& shaderPath);
    };

} // namespace ettycc


#endif