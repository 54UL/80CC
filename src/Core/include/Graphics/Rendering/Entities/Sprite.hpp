#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP

#include "../../Shading/ShaderPipeline.hpp"
#include "../Renderable.hpp"
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>

#include <memory>
#include <string>
#include <GL/glew.h>
#include <GL/gl.h>
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

        // Tiling multiplier applied on top of the scale-relative tiling.
        // 1.0 = the checker (or any texture) tiles once per world unit of scale.
        // 2.0 = twice as dense, 0.5 = half as dense, etc.
        float tilingMultiplier_ = 1.0f;

    public:
        ShaderPipeline underlyingShader;

    public:
        Sprite();
        Sprite(const std::string &spriteFilePath, bool initialize);
        Sprite(const std::string &spriteFilePath);

        ~Sprite();

        void InitBackend(const std::string &spritePath);
        void LoadShaders();
        void LoadTextures(const std::string &spritePath);

        // Renderable
    public:
        void Init(const std::shared_ptr<Engine> &engineCtx) override;
        void Pass(const std::shared_ptr<RenderingContext> &ctx, float time) override;
        void DrawForPicker(const std::shared_ptr<RenderingContext>& ctx,
                           GLuint program, uint32_t id) override;
        void Inspect(EditorPropertyVisitor& v) override;

        // Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::base_class<Renderable>(this),
               CEREAL_NVP(spriteFilePath_),
               CEREAL_NVP(tilingMultiplier_));
        }

        // Internal usage
    private:
        std::string LoadShaderFile(const std::string &shaderPath);
    };

} // namespace ettycc

#endif