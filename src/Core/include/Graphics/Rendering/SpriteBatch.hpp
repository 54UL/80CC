#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>
#include <vector>
#include <unordered_map>
#include <memory>

namespace ettycc
{
    class Sprite;
    struct CachedShader;

    // ── SpriteBatch ──────────────────────────────────────────────────────────
    // Batched instanced renderer for Sprite renderables.
    //
    // Two rendering paths:
    //   1. Instanced — sprites sharing the same (shader, texture, shape preset)
    //      are drawn with one glDrawElementsInstanced call.
    //   2. Base-vertex mega-buffer — sprites with custom/edited geometry are
    //      packed into a shared VBO and drawn with glMultiDrawElementsBaseVertex.
    //
    // Per-instance data (model matrix + tiling) is uploaded via an instance VBO
    // with glVertexAttribDivisor.  The instanced shader reads the model matrix
    // from vertex attributes instead of a uniform.
    class SpriteBatch
    {
    public:
        SpriteBatch();
        ~SpriteBatch();

        // Non-copyable.
        SpriteBatch(const SpriteBatch&) = delete;
        SpriteBatch& operator=(const SpriteBatch&) = delete;

        // Call once after GL context is ready (loads instanced shader).
        void Init();

        // ── Per-frame API ─────────────────────────────────────────────────────
        // Collect sprites, build batches, render, clear.
        void Begin(const std::shared_ptr<RenderingContext>& ctx, float dt);
        void Submit(Sprite* sprite);
        void End();

    private:
        // Per-instance data uploaded to the GPU.
        struct alignas(16) InstanceData
        {
            glm::mat4 model;    // 64 bytes
            glm::vec2 tiling;   // 8 bytes
            float     _pad[2];  // 8 bytes → 80 bytes total (aligned)
        };

        // A batch groups sprites that can share ONE draw call.
        struct BatchKey
        {
            GLuint shader;
            GLuint texture;
            int    shapePreset; // SpriteShape::Preset as int (-1 = custom)

            bool operator==(const BatchKey& o) const
            {
                return shader == o.shader && texture == o.texture
                    && shapePreset == o.shapePreset;
            }
        };

        struct BatchKeyHash
        {
            size_t operator()(const BatchKey& k) const
            {
                size_t h = std::hash<GLuint>()(k.shader);
                h ^= std::hash<GLuint>()(k.texture) * 2654435761u;
                h ^= std::hash<int>()(k.shapePreset) * 40499u;
                return h;
            }
        };

        struct Batch
        {
            std::vector<InstanceData> instances;
            GLuint texture = 0;
        };

        // ── Shared preset geometry (one VAO per shape preset) ─────────────────
        struct PresetGeo
        {
            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint ebo = 0;
            int    indexCount = 0;
        };

        void BuildPresetGeo(int preset);
        PresetGeo& GetPresetGeo(int preset);

        // ── Custom geometry mega-buffer ───────────────────────────────────────
        struct CustomEntry
        {
            Sprite*      sprite;
            InstanceData instance;
        };

        void RenderCustomSprites();

        // ── GL resources ──────────────────────────────────────────────────────
        std::unordered_map<int, PresetGeo> presetGeos_;

        GLuint instanceVBO_ = 0;    // reused across batches each frame
        size_t instanceVBOCapacity_ = 0;

        // Mega-buffer for custom geometry
        GLuint customVAO_ = 0;
        GLuint customVBO_ = 0;
        GLuint customEBO_ = 0;
        GLuint customInstanceVBO_ = 0;
        size_t customVBOCapacity_ = 0;
        size_t customEBOCapacity_ = 0;

        // Instanced shader (shared, loaded once)
        std::shared_ptr<CachedShader> instancedShader_;

        // ── Per-frame state ───────────────────────────────────────────────────
        std::unordered_map<BatchKey, Batch, BatchKeyHash> batches_;
        std::vector<CustomEntry> customSprites_;
        std::shared_ptr<RenderingContext> ctx_;
        float dt_ = 0.f;

        bool initialized_ = false;

        void EnsureInstanceVBO(size_t count);
        void SetupInstanceAttributes(GLuint vao);
    };

} // namespace ettycc
