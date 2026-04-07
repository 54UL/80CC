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
        static constexpr const char* kDefaultShader = "sprite";

    private:
        GLuint VAO = 0, VBO = 0, EBO = 0;
        GLuint TEXTURE = 0;            // cached GL handle (owned by ResourceCache)

        // Material path (.material file, relative to working folder)
        std::string materialPath_; //Stop using this as an indicator, should come from the cache as n id

        // Resolved from material (or set directly for backwards compat)
        std::string spriteFilePath_;   // texture path (relative)
        std::string shaderName_;       // shader base name, (TODO: FETCH FROM MATERIAL LOCALTED IN CACHE)

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

        void InitBackend(const std::string &texturePath, const std::string& shaderName);

        // Apply a new shape and rebuild GL buffers (only if already initialized)
        void SetShape(const SpriteShape& shape);
        const SpriteShape& GetShape() const { return shape_; }

        // Material assignment (TODO: USE INSTEAD AN ID)
        void SetMaterialPath(const std::string& path) { materialPath_ = path; }
        const std::string& GetMaterialPath() const { return materialPath_; }

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
        void ResolveMaterial();  // loads .material file and fills spriteFilePath_ + shaderName_

    public:
        // Serialization/Deserialization
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::base_class<Renderable>(this),
               CEREAL_NVP(materialPath_),
               CEREAL_NVP(spriteFilePath_),
               CEREAL_NVP(shaderName_),
               CEREAL_NVP(tilingMultiplier_),
               CEREAL_NVP(shape_));
        }
    };

} // namespace ettycc

#endif