#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP

#include "../Renderable.hpp"
#include "SpriteShape.hpp"
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
        float tilingMultiplier_ = 1.0f;

        // Shape geometry — defaults to quad for backwards compatibility
        SpriteShape shape_ = SpriteShape::MakeQuad();
        int indexCount_ = 6;

        // Shared shader program from ResourceCache (not owned by this instance)
        std::shared_ptr<CachedShader> cachedShader_;

    public:
        Sprite();
        Sprite(const std::string &spriteFilePath, bool initialize);
        Sprite(const std::string &spriteFilePath);

        ~Sprite();

        void InitBackend(const std::string &spritePath);

        // Apply a new shape and rebuild GL buffers (only if already initialized)
        void SetShape(const SpriteShape& shape);
        const SpriteShape& GetShape() const { return shape_; }

        // Renderable
    public:
        void Init(const std::shared_ptr<Engine> &engineCtx) override;
        void Pass(const std::shared_ptr<RenderingContext> &ctx, float time) override;
        void DrawForPicker(const std::shared_ptr<RenderingContext>& ctx,
                           GLuint program, uint32_t id) override;
        void Inspect(EditorPropertyVisitor& v) override;

        GLuint GetShaderProgramId() const;
        const std::string& GetTexturePath() const { return spriteFilePath_; }

    private:
        void UploadGeometry();

    public:
        // Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::base_class<Renderable>(this),
               CEREAL_NVP(spriteFilePath_),
               CEREAL_NVP(tilingMultiplier_),
               CEREAL_NVP(shape_));
        }
    };

} // namespace ettycc

#endif