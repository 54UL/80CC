#ifndef RENDERING_MESH_HPP
#define RENDERING_MESH_HPP

#include "../Renderable.hpp"
#include "SpriteShape.hpp"
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Scene/Assets/AssetHandle.hpp>
#include <Scene/Assets/MaterialAsset.hpp>
#include <Scene/Assets/ShaderAsset.hpp>
#include <Scene/Assets/TextureAsset.hpp>

#include <memory>
#include <string>
#include <GL/glew.h>
#include <GL/gl.h>
#include <spdlog/spdlog.h>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    // Legacy struct kept for backward-compatible deserialization.
    // New code should use material uniforms instead.
    struct ProceduralRockParams
    {
        glm::vec3 baseColor  = { 0.45f, 0.42f, 0.40f };
        glm::vec3 edgeColor  = { 0.15f, 0.13f, 0.12f };
        float     edgeWidth  = 0.15f;

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(cereal::make_nvp("baseR", baseColor.r),
               cereal::make_nvp("baseG", baseColor.g),
               cereal::make_nvp("baseB", baseColor.b),
               cereal::make_nvp("edgeR", edgeColor.r),
               cereal::make_nvp("edgeG", edgeColor.g),
               cereal::make_nvp("edgeB", edgeColor.b),
               cereal::make_nvp("edgeWidth", edgeWidth));
        }

        // Convert to material uniform map
        std::unordered_map<std::string, UniformValue> ToUniforms() const
        {
            return {
                {"uBaseColor", baseColor},
                {"uEdgeColor", edgeColor},
                {"uEdgeWidth", edgeWidth}
            };
        }
    };

    class Sprite : public Renderable
    {
        static constexpr const char* kDefaultShader = "sprite";

    private:
        GLuint VAO = 0, VBO = 0, EBO = 0;

        // Material handle -- the single source of truth for texture + shader + uniforms.
        AssetHandle<MaterialAsset> materialHandle_;

        // Serialized paths (used to resolve handles at Init time)
        std::string materialPath_;     // .material file (relative), may be empty
        std::string spriteFilePath_;   // texture path (relative), for backwards compat
        std::string shaderName_;       // shader base name, for backwards compat

        // Tiling multiplier applied on top of the scale-relative tiling.
        float tilingMultiplier_ = 1.0f;

        // Shape geometry -- defaults to quad for backwards compatibility
        SpriteShape shape_ = SpriteShape::MakeQuad();
        int indexCount_ = 6;

        // Legacy fields kept for backward-compatible deserialization.
        // On Init(), these are migrated into the material's uniform map.
        bool procedural_ = false;
        ProceduralRockParams proceduralParams_;

        // Pending uniforms from MakeWithShader() -- consumed by ResolveMaterial()
        std::unordered_map<std::string, UniformValue> pendingUniforms_;

    public:
        Sprite();
        Sprite(const std::string &spriteFilePath, bool initialize);
        Sprite(const std::string &spriteFilePath);

        ~Sprite();

        // Create a textureless sprite with a named shader and optional uniforms.
        // The sprite is shader-agnostic -- any shader name can be passed.
        static std::shared_ptr<Sprite> MakeWithShader(const SpriteShape& shape,
                                                       const std::string& shaderName,
                                                       const std::unordered_map<std::string, UniformValue>& uniforms = {});

        // Legacy alias -- callers should migrate to MakeWithShader.
        static std::shared_ptr<Sprite> MakeProcedural(const SpriteShape& shape,
                                                       const ProceduralRockParams& params = {});

        void InitBackend();

        // Apply a new shape and rebuild GL buffers (only if already initialized)
        void SetShape(const SpriteShape& shape);
        const SpriteShape& GetShape() const { return shape_; }

        // Material assignment by handle
        void SetMaterial(AssetHandle<MaterialAsset> h) { materialHandle_ = h; }
        AssetHandle<MaterialAsset> GetMaterial() const { return materialHandle_; }

        // Material path (for serialization / drag-drop)
        void SetMaterialPath(const std::string& path) { materialPath_ = path; }
        const std::string& GetMaterialPath() const { return materialPath_; }

        // Query: does this sprite's material have a texture?
        bool IsTextureless() const;

        // Legacy compat -- maps to IsTextureless()
        bool IsProcedural() const { return IsTextureless(); }

        // Access material uniforms (replaces ProceduralRockParams getters)
        const ProceduralRockParams& GetProceduralParams() const { return proceduralParams_; }

        // Renderable
    public:
        void Init(const std::shared_ptr<Engine> &engineCtx) override;
        void Pass(const std::shared_ptr<RenderingContext> &ctx, float time) override;
        void DrawForPicker(const std::shared_ptr<RenderingContext>& ctx,
                           GLuint program, uint32_t id) override;
        void Inspect(EditorPropertyVisitor& v) override;

        // Resolve material handle -> GL handles for rendering.
        GLuint GetShaderProgramId() const;
        GLuint GetTextureHandle() const;
        float  GetTilingMultiplier() const { return tilingMultiplier_; }
        TextureWrapMode GetTextureWrapMode() const;
        bool   IsCustomGeometry() const { return shape_.preset == SpriteShape::Preset::Custom; }

        // For scene asset preloading -- returns the texture relative path
        const std::string& GetTexturePath() const { return spriteFilePath_; }
        const std::string& GetShaderName() const { return shaderName_; }

    private:
        void UploadGeometry();
        void ResolveMaterial();

    public:
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::base_class<Renderable>(this),
               CEREAL_NVP(materialPath_),
               CEREAL_NVP(spriteFilePath_),
               CEREAL_NVP(shaderName_),
               CEREAL_NVP(tilingMultiplier_),
               CEREAL_NVP(shape_),
               CEREAL_NVP(procedural_),
               CEREAL_NVP(proceduralParams_));
        }
    };

} // namespace ettycc

#endif
