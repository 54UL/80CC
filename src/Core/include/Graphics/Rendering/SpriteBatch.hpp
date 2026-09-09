#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>
#include <vector>
#include <unordered_map>
#include <memory>
#include <Scene/Assets/AssetHandle.hpp>
#include <Scene/Assets/ShaderAsset.hpp>
#include <Scene/Assets/MaterialAsset.hpp>

namespace ettycc { class ThreadRegistry; }

namespace ettycc
{
    class Sprite;
    struct SpriteShape;
    struct ShaderAsset;
    struct MaterialAsset;
    template<typename T> struct AssetHandle;

    // -- SpriteBatch ----------------------------------------------------------
    // Batched instanced renderer for Sprite renderables.
    //
    // Two rendering paths:
    //   1. Instanced -- sprites sharing the same (shader, texture, shape preset)
    //      are drawn with one glDrawElementsInstanced call.
    //   2. Base-vertex mega-buffer -- sprites with custom/edited geometry are
    //      packed into a shared VBO and drawn with glDrawElementsBaseVertex.
    //
    // Per-instance data (model matrix + tiling) is uploaded via an instance VBO
    // with glVertexAttribDivisor.  The instanced shader reads the model matrix
    // from vertex attributes instead of a uniform.
    //
    // Material-driven: shader and uniforms come from the sprite's material.
    // Textureless materials (procedural shaders) are handled transparently.
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

        // -- Per-frame API -----------------------------------------------------
        // Collect sprites, build batches, render, clear.
        void Begin(const std::shared_ptr<RenderingContext>& ctx, float dt);
        void Submit(Sprite* sprite);
        void End();

    private:
        // Per-instance data uploaded to the GPU (textured sprites).
        struct alignas(16) InstanceData
        {
            glm::mat4 model;    // 64 bytes
            glm::vec2 tiling;   // 8 bytes
            float     _pad[2];  // 8 bytes -> 80 bytes total (aligned)
        };

        // Per-instance data for procedural sprites (includes material params).
        struct alignas(16) ProceduralInstanceData
        {
            glm::mat4 model;      // 64 bytes  (locations 2-5)
            glm::vec4 baseColor;  // 16 bytes  (location 6) -- xyz = color, w = edgeWidth
            glm::vec4 edgeColor;  // 16 bytes  (location 7) -- xyz = color, w = unused
        }; // 96 bytes total

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

        // -- Shared preset geometry (one VAO per shape preset) -----------------
        struct PresetGeo
        {
            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint ebo = 0;
            int    indexCount = 0;
        };

        void BuildPresetGeo(int preset);
        PresetGeo& GetPresetGeo(int preset);

        // -- Custom geometry mega-buffer ---------------------------------------
        struct CustomEntry
        {
            Sprite*      sprite;
            InstanceData instance;
        };

        void RenderCustomSprites();
        void RenderProceduralMegaDraw();

        // -- GL resources ------------------------------------------------------
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

        // Mega-buffer for procedural geometry (separate from custom to avoid layout conflicts)
        GLuint procVAO_ = 0;
        GLuint procVBO_ = 0;
        GLuint procEBO_ = 0;
        size_t procVBOCapacity_ = 0;
        size_t procEBOCapacity_ = 0;

        // Persistent mapped buffer (triple-buffered, zero-copy GPU upload)
        static constexpr int kProcFrames = 3;
        float*        procMappedVBO_ = nullptr;   // persistent map of procVBO_
        unsigned int* procMappedEBO_ = nullptr;   // persistent map of procEBO_
        size_t procPersistentVBOCap_ = 0;         // per-frame capacity (bytes)
        size_t procPersistentEBOCap_ = 0;         // per-frame capacity (bytes)
        int    procFrameIdx_         = 0;          // current frame slot [0..2]
        GLsync procFences_[kProcFrames] = {};      // per-frame fence objects
        bool   procPersistentReady_  = false;      // true once glBufferStorage succeeded

        // Instanced shader handles (resolved from AssetRegistry)
        AssetHandle<ShaderAsset> shaderHandle_;
        GLuint                   instancedProgramId_ = 0;

        // Procedural rock instanced shader
        AssetHandle<ShaderAsset> proceduralShaderHandle_;
        GLuint                   proceduralProgramId_ = 0;

        // -- SoA storage for procedural sprites (hot path) ----------------------
        // Populated at Submit() time so RenderProceduralMegaDraw() reads contiguous
        // memory instead of chasing Sprite* pointers. Eliminates Phase 1 gather.
        struct ProceduralSoA
        {
            // Per-sprite transform (2D matrix cols -- 9 floats each)
            std::vector<float> m00, m10, m30;  // column 0
            std::vector<float> m01, m11, m31;  // column 1
            std::vector<float> m02, m12, m32;  // column 2 (depth)

            // Per-sprite material params
            std::vector<float> br, bg, bb;     // base color
            std::vector<float> er, eg, eb, ew; // edge color + width

            // Per-sprite shape data (stable pointers within Begin/End frame)
            struct ShapeRef {
                const SpriteShape* shape;
            };
            std::vector<ShapeRef> shapes;

            size_t count = 0;

            void Clear()
            {
                m00.clear(); m10.clear(); m30.clear();
                m01.clear(); m11.clear(); m31.clear();
                m02.clear(); m12.clear(); m32.clear();
                br.clear();  bg.clear();  bb.clear();
                er.clear();  eg.clear();  eb.clear(); ew.clear();
                shapes.clear();
                count = 0;
            }

            void Reserve(size_t n)
            {
                m00.reserve(n); m10.reserve(n); m30.reserve(n);
                m01.reserve(n); m11.reserve(n); m31.reserve(n);
                m02.reserve(n); m12.reserve(n); m32.reserve(n);
                br.reserve(n);  bg.reserve(n);  bb.reserve(n);
                er.reserve(n);  eg.reserve(n);  eb.reserve(n); ew.reserve(n);
                shapes.reserve(n);
            }
        };

        // -- Per-frame state ---------------------------------------------------
        std::unordered_map<BatchKey, Batch, BatchKeyHash> batches_;
        std::vector<CustomEntry> customSprites_;       // non-procedural custom geo
        ProceduralSoA procSoA_;                        // procedural sprites (SoA layout)
        std::shared_ptr<RenderingContext> ctx_;
        float dt_ = 0.f;

        // Persistent staging buffers (reused across frames to avoid alloc/dealloc)
        std::vector<float>        procStagingVerts_;
        std::vector<unsigned int> procStagingIndices_;

        bool initialized_ = false;

        void EnsureInstanceVBO(size_t count);
        void SetupInstanceAttributes(GLuint vao);
        void SetupProceduralVertexAttribs(size_t byteOffset = 0);

    public:
        // -- Per-frame stats (read by debug UI) --------------------------------
        struct Stats
        {
            int   drawCalls     = 0;
            int   spriteCount   = 0;
            int   vertexCount   = 0;
            int   triangleCount = 0;
            float cpuBuildMs    = 0.f;  // mega-buffer construction time
            float cpuUploadMs   = 0.f;  // GL buffer upload time
            float gpuMs         = 0.f;  // GPU time (timer query, 1-frame latency)
        };
        const Stats& GetStats() const { return stats_; }

    private:
        Stats stats_;

        // GPU timer queries (double-buffered for async readback)
        GLuint gpuTimerQuery_[2] = {0, 0};
        int    gpuTimerFrame_    = 0;
        bool   gpuTimerReady_    = false;

        // Cached for parallel mega-buffer construction
        ThreadRegistry* threadRegistry_ = nullptr;

        // -- Compute shader resources (GL 4.5) ---------------------------------
        GLuint computeVertProg_  = 0;  // sprite_transform.comp
        GLuint computeIdxProg_   = 0;  // sprite_index_transform.comp
        GLuint ssboSpriteInfo_   = 0;  // per-sprite transform + material
        GLuint ssboSrcVerts_     = 0;  // source vertices (local pos + UV)
        GLuint ssboSrcIndices_   = 0;  // source indices
        GLuint ssboVertToSprite_ = 0;  // maps each output vertex → sprite index
        GLuint ssboIdxToSprite_  = 0;  // maps each output index → sprite index
        GLuint ssboDstVerts_     = 0;  // output transformed vertices (= render VBO)
        GLuint ssboDstIndices_   = 0;  // output offset indices (= render EBO)
        size_t ssboSpriteInfoCap_   = 0;
        size_t ssboSrcVertsCap_     = 0;
        size_t ssboSrcIndicesCap_   = 0;
        size_t ssboVertToSpriteCap_ = 0;
        size_t ssboIdxToSpriteCap_  = 0;
        size_t ssboDstVertsCap_     = 0;
        size_t ssboDstIndicesCap_   = 0;
        bool   computeReady_        = false;

        void InitComputeShaders();
        void RenderProceduralCompute(); // compute shader path
    };

} // namespace ettycc
