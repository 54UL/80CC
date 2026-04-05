#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP

#include "../Renderable.hpp"
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Scene/Assets/ResourceCache.hpp>

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
        static constexpr const char* kShaderName = "sprite";

    private:
        GLuint VAO = 0, VBO = 0, EBO = 0;
        GLuint TEXTURE = 0;            // cached GL handle (owned by ResourceCache)
        std::string spriteFilePath_;

        // Tiling multiplier applied on top of the scale-relative tiling.
        // 1.0 = the checker (or any texture) tiles once per world unit of scale.
        // 2.0 = twice as dense, 0.5 = half as dense, etc.
        float tilingMultiplier_ = 1.0f;

        // Shared shader program from ResourceCache (not owned by this instance)
        std::shared_ptr<CachedShader> cachedShader_;

    public:
        Sprite();
        Sprite(const std::string &spriteFilePath, bool initialize);
        Sprite(const std::string &spriteFilePath);

        ~Sprite();

        void InitBackend(const std::string &spritePath);

        // Renderable
    public:
        void Init(const std::shared_ptr<Engine> &engineCtx) override;
        void Pass(const std::shared_ptr<RenderingContext> &ctx, float time) override;
        void DrawForPicker(const std::shared_ptr<RenderingContext>& ctx,
                           GLuint program, uint32_t id) override;
        void Inspect(EditorPropertyVisitor& v) override;

        GLuint GetShaderProgramId() const;
        const std::string& GetTexturePath() const { return spriteFilePath_; }

        // Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::base_class<Renderable>(this),
               CEREAL_NVP(spriteFilePath_),
               CEREAL_NVP(tilingMultiplier_));
        }
    };

} // namespace ettycc

#endif