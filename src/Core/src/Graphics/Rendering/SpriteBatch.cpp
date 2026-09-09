#include <Graphics/Rendering/SpriteBatch.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Graphics/Rendering/Entities/SpriteShape.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>
#include <Scene/Assets/AssetRegistry.hpp>
#include <Threading/ThreadRegistry.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Dependency.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>

namespace ettycc
{

// --- Lifecycle --------------------------------------------------------------

SpriteBatch::SpriteBatch()  = default;
SpriteBatch::~SpriteBatch()
{
    if (instanceVBO_)       glDeleteBuffers(1, &instanceVBO_);
    if (customVAO_)         glDeleteVertexArrays(1, &customVAO_);
    if (customVBO_)         glDeleteBuffers(1, &customVBO_);
    if (customEBO_)         glDeleteBuffers(1, &customEBO_);
    if (customInstanceVBO_) glDeleteBuffers(1, &customInstanceVBO_);

    // Clean up persistent mapped buffers
    if (procPersistentReady_)
    {
        for (int f = 0; f < kProcFrames; ++f)
            if (procFences_[f]) glDeleteSync(procFences_[f]);
        if (procVBO_) { glBindBuffer(GL_ARRAY_BUFFER, procVBO_); glUnmapBuffer(GL_ARRAY_BUFFER); }
        if (procEBO_) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, procEBO_); glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER); }
    }

    if (procVAO_)           glDeleteVertexArrays(1, &procVAO_);
    if (procVBO_)           glDeleteBuffers(1, &procVBO_);
    if (procEBO_)           glDeleteBuffers(1, &procEBO_);
    if (gpuTimerQuery_[0])  glDeleteQueries(2, gpuTimerQuery_);

    // Compute shader cleanup
    if (computeVertProg_)   glDeleteProgram(computeVertProg_);
    if (computeIdxProg_)    glDeleteProgram(computeIdxProg_);
    if (ssboSpriteInfo_)    glDeleteBuffers(1, &ssboSpriteInfo_);
    if (ssboSrcVerts_)      glDeleteBuffers(1, &ssboSrcVerts_);
    if (ssboSrcIndices_)    glDeleteBuffers(1, &ssboSrcIndices_);
    if (ssboVertToSprite_)  glDeleteBuffers(1, &ssboVertToSprite_);
    if (ssboIdxToSprite_)   glDeleteBuffers(1, &ssboIdxToSprite_);
    if (ssboDstVerts_)      glDeleteBuffers(1, &ssboDstVerts_);
    if (ssboDstIndices_)    glDeleteBuffers(1, &ssboDstIndices_);

    for (auto& [preset, geo] : presetGeos_)
    {
        if (geo.vao) glDeleteVertexArrays(1, &geo.vao);
        if (geo.vbo) glDeleteBuffers(1, &geo.vbo);
        if (geo.ebo) glDeleteBuffers(1, &geo.ebo);
    }
}

void SpriteBatch::Init()
{
    if (initialized_) return;

    // Load the instanced shader via AssetRegistry
    auto registry = GetDependency(AssetRegistry);
    shaderHandle_ = registry->GetShader("sprite_instanced");
    auto* shader  = registry->Shaders().Get(shaderHandle_);
    if (!shader)
    {
        spdlog::error("[SpriteBatch] Failed to load sprite_instanced shader");
        return;
    }

    instancedProgramId_ = shader->programId;

    // Set texture sampler uniform once
    shader->pipeline.Bind();
    glUniform1i(glGetUniformLocation(instancedProgramId_, "ourTexture"), 0);
    shader->pipeline.Unbind();

    // Load procedural rock instanced shader
    proceduralShaderHandle_ = registry->GetShader("rock_procedural_instanced");
    auto* procShader = registry->Shaders().Get(proceduralShaderHandle_);
    if (procShader)
        proceduralProgramId_ = procShader->programId;
    else
        spdlog::warn("[SpriteBatch] rock_procedural_instanced shader not found -- procedural sprites will use fallback path");

    // Create shared instance VBO (will grow as needed)
    glGenBuffers(1, &instanceVBO_);

    // Create custom geometry mega-buffer resources
    glGenVertexArrays(1, &customVAO_);
    glGenBuffers(1, &customVBO_);
    glGenBuffers(1, &customEBO_);
    glGenBuffers(1, &customInstanceVBO_);

    // Create procedural geometry mega-buffer resources (separate VAO/layout)
    glGenVertexArrays(1, &procVAO_);
    glGenBuffers(1, &procVBO_);
    glGenBuffers(1, &procEBO_);

    // Cache ThreadRegistry for parallel mega-buffer construction
    auto engine = GetDependency(Engine);
    if (engine) threadRegistry_ = &engine->threadRegistry_;

    initialized_ = true;
    spdlog::info("[SpriteBatch] Initialized (instanced shader program {})",
                 instancedProgramId_);
}

// --- Per-frame API ----------------------------------------------------------

void SpriteBatch::Begin(const std::shared_ptr<RenderingContext>& ctx, float dt)
{
    ctx_ = ctx;
    dt_  = dt;
    batches_.clear();
    customSprites_.clear();
    procSoA_.Clear();
}

void SpriteBatch::Submit(Sprite* sprite)
{
    if (!sprite || !sprite->initialized) return;

    const SpriteShape& shape = sprite->GetShape();
    const bool isCustom = (shape.preset == SpriteShape::Preset::Custom);

    // Procedural (textureless) sprites: SoA fast path with mega-draw.
    // Extract all hot data NOW so RenderProceduralMegaDraw() reads contiguous memory.
    if (sprite->IsTextureless())
    {
        const glm::mat4& M = sprite->transform().GetMatrix();

        procSoA_.m00.push_back(M[0][0]); procSoA_.m10.push_back(M[1][0]); procSoA_.m30.push_back(M[3][0]);
        procSoA_.m01.push_back(M[0][1]); procSoA_.m11.push_back(M[1][1]); procSoA_.m31.push_back(M[3][1]);
        procSoA_.m02.push_back(M[0][2]); procSoA_.m12.push_back(M[1][2]); procSoA_.m32.push_back(M[3][2]);

        // Extract material params inline (avoids chasing pointer at render time)
        float br = 0.45f, bg = 0.42f, bb = 0.40f;
        float er = 0.15f, eg = 0.13f, eb = 0.12f, ew = 0.15f;

        auto registry = GetDependency(AssetRegistry);
        auto* mat = registry->Materials().Get(sprite->GetMaterial());
        if (mat)
        {
            auto itB = mat->uniforms.find("uBaseColor");
            if (itB != mat->uniforms.end() && std::holds_alternative<glm::vec3>(itB->second))
            {
                auto& c = std::get<glm::vec3>(itB->second);
                br = c.r; bg = c.g; bb = c.b;
            }
            auto itE = mat->uniforms.find("uEdgeColor");
            if (itE != mat->uniforms.end() && std::holds_alternative<glm::vec3>(itE->second))
            {
                auto& c = std::get<glm::vec3>(itE->second);
                er = c.r; eg = c.g; eb = c.b;
            }
            auto itW = mat->uniforms.find("uEdgeWidth");
            if (itW != mat->uniforms.end() && std::holds_alternative<float>(itW->second))
                ew = std::get<float>(itW->second);
        }

        procSoA_.br.push_back(br); procSoA_.bg.push_back(bg); procSoA_.bb.push_back(bb);
        procSoA_.er.push_back(er); procSoA_.eg.push_back(eg); procSoA_.eb.push_back(eb);
        procSoA_.ew.push_back(ew);

        procSoA_.shapes.push_back({ &sprite->GetShape() });
        ++procSoA_.count;
        return;
    }

    if (isCustom)
    {
        InstanceData inst{};
        inst.model  = sprite->transform().GetMatrix();
        inst.tiling = glm::vec2(1.0f, 1.0f);
        customSprites_.push_back({ sprite, inst });
        return;
    }

    // Build per-instance data for textured preset sprites
    InstanceData inst{};
    inst.model  = sprite->transform().GetMatrix();

    if (sprite->GetTextureWrapMode() == TextureWrapMode::FitToShape)
    {
        inst.tiling = glm::vec2(1.0f, 1.0f);
    }
    else
    {
        const glm::vec3 scale = sprite->transform().getGlobalScale();
        inst.tiling = glm::vec2(glm::abs(scale.x) * sprite->GetTilingMultiplier(),
                                glm::abs(scale.y) * sprite->GetTilingMultiplier());
    }

    BatchKey key{};
    key.shader      = instancedProgramId_;
    key.texture     = sprite->GetTextureHandle();
    key.shapePreset = static_cast<int>(shape.preset);

    auto& batch     = batches_[key];
    batch.texture   = key.texture;
    batch.instances.push_back(inst);
}

void SpriteBatch::End()
{
    if (!initialized_ || instancedProgramId_ == 0) return;

    // Reset per-frame stats
    stats_ = {};
    stats_.spriteCount = static_cast<int>(procSoA_.count
                         + customSprites_.size());
    for (auto& [k, b] : batches_)
        stats_.spriteCount += static_cast<int>(b.instances.size());

    auto registry = GetDependency(AssetRegistry);
    auto* shader  = registry->Shaders().Get(shaderHandle_);
    if (!shader) return;

    const glm::mat4 PV = ctx_->Projection * ctx_->View;
    const GLuint prog   = instancedProgramId_;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);

    shader->pipeline.Bind();
    glUniformMatrix4fv(glGetUniformLocation(prog, "uPV"), 1, GL_FALSE, glm::value_ptr(PV));

    // -- 1. Instanced path: one draw call per (texture, shape) group ---------
    for (auto& [key, batch] : batches_)
    {
        if (batch.instances.empty()) continue;

        PresetGeo& geo = GetPresetGeo(key.shapePreset);

        EnsureInstanceVBO(batch.instances.size());
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(batch.instances.size() * sizeof(InstanceData)),
                        batch.instances.data());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.texture);
        glBindVertexArray(geo.vao);

        glDrawElementsInstanced(GL_TRIANGLES, geo.indexCount, GL_UNSIGNED_INT,
                                nullptr, static_cast<GLsizei>(batch.instances.size()));
        ++stats_.drawCalls;
        stats_.triangleCount += (geo.indexCount / 3) * static_cast<int>(batch.instances.size());

        glBindVertexArray(0);
    }

    // -- 2. Procedural mega-draw (1 draw call for all procedural rocks) -----
    if (procSoA_.count > 0)
        RenderProceduralMegaDraw();

    // -- 3. Custom geometry path (non-procedural custom shapes, rare) ------
    if (!customSprites_.empty())
    {
        stats_.drawCalls += static_cast<int>(customSprites_.size());
        RenderCustomSprites();
    }

    shader->pipeline.Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);

    // Restore GL state
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);

    ctx_.reset();
}

// --- Preset geometry --------------------------------------------------------

void SpriteBatch::BuildPresetGeo(int preset)
{
    SpriteShape shape;
    switch (static_cast<SpriteShape::Preset>(preset))
    {
        case SpriteShape::Preset::Quad:     shape = SpriteShape::MakeQuad();     break;
        case SpriteShape::Preset::Triangle: shape = SpriteShape::MakeTriangle(); break;
        case SpriteShape::Preset::Circle:   shape = SpriteShape::MakeCircle();   break;
        default: return;
    }

    PresetGeo geo{};
    auto vbuf = shape.BuildVertexBuffer();
    geo.indexCount = static_cast<int>(shape.indices.size());

    glGenVertexArrays(1, &geo.vao);
    glGenBuffers(1, &geo.vbo);
    glGenBuffers(1, &geo.ebo);

    glBindVertexArray(geo.vao);

    glBindBuffer(GL_ARRAY_BUFFER, geo.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vbuf.size() * sizeof(float)),
                 vbuf.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(shape.indices.size() * sizeof(unsigned int)),
                 shape.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    SetupInstanceAttributes(geo.vao);

    glBindVertexArray(0);

    presetGeos_[preset] = geo;
}

SpriteBatch::PresetGeo& SpriteBatch::GetPresetGeo(int preset)
{
    auto it = presetGeos_.find(preset);
    if (it == presetGeos_.end())
    {
        BuildPresetGeo(preset);
        it = presetGeos_.find(preset);
    }
    return it->second;
}

// --- Custom geometry (mega-buffer) ------------------------------------------

void SpriteBatch::RenderCustomSprites()
{
    static float elapsedTime = 0.f;
    elapsedTime += dt_;

    auto registry = GetDependency(AssetRegistry);

    // Sort by shader program, then by texture handle for minimal state changes
    std::sort(customSprites_.begin(), customSprites_.end(),
              [](const CustomEntry& a, const CustomEntry& b)
              {
                  GLuint shaderA = a.sprite->GetShaderProgramId();
                  GLuint shaderB = b.sprite->GetShaderProgramId();
                  if (shaderA != shaderB) return shaderA < shaderB;
                  return a.sprite->GetTextureHandle() < b.sprite->GetTextureHandle();
              });

    // Pack all custom sprite geometry into a mega-buffer for efficient drawing
    std::vector<float>        megaVerts;
    std::vector<unsigned int> megaIndices;

    std::vector<GLsizei> counts;
    std::vector<void*>   offsets;
    std::vector<GLint>   baseVertices;

    size_t vertexOffset = 0;
    size_t indexOffset   = 0;

    for (auto& entry : customSprites_)
    {
        const SpriteShape& shape = entry.sprite->GetShape();
        auto vbuf = shape.BuildVertexBuffer();

        megaVerts.insert(megaVerts.end(), vbuf.begin(), vbuf.end());
        megaIndices.insert(megaIndices.end(), shape.indices.begin(), shape.indices.end());

        counts.push_back(static_cast<GLsizei>(shape.indices.size()));
        offsets.push_back(reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)));
        baseVertices.push_back(static_cast<GLint>(vertexOffset));

        vertexOffset += shape.vertices.size();
        indexOffset  += shape.indices.size();
    }

    glBindVertexArray(customVAO_);

    glBindBuffer(GL_ARRAY_BUFFER, customVBO_);
    size_t vboBytes = megaVerts.size() * sizeof(float);
    if (vboBytes > customVBOCapacity_)
    {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vboBytes),
                     megaVerts.data(), GL_DYNAMIC_DRAW);
        customVBOCapacity_ = vboBytes;
    }
    else
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vboBytes), megaVerts.data());
    }

    // Vertex attributes: position (loc 0) + texcoord (loc 1)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, customEBO_);
    size_t eboBytes = megaIndices.size() * sizeof(unsigned int);
    if (eboBytes > customEBOCapacity_)
    {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(eboBytes),
                     megaIndices.data(), GL_DYNAMIC_DRAW);
        customEBOCapacity_ = eboBytes;
    }
    else
    {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(eboBytes), megaIndices.data());
    }

    GLuint currentShader = 0;
    GLuint currentTex = 0;
    const glm::mat4 PV = ctx_->Projection * ctx_->View;

    for (size_t i = 0; i < customSprites_.size(); ++i)
    {
        Sprite* sprite = customSprites_[i].sprite;
        GLuint spriteShader = sprite->GetShaderProgramId();
        GLuint spriteTex    = sprite->GetTextureHandle();

        // Switch shader if needed
        if (spriteShader != currentShader)
        {
            currentShader = spriteShader;
            glUseProgram(currentShader);
            // Set shared time uniforms once per shader switch
            glUniform1f(glGetUniformLocation(currentShader, "time"), elapsedTime);
            glUniform1f(glGetUniformLocation(currentShader, "deltaTime"), dt_);
        }

        // Per-sprite PVM uniform (non-instanced shaders use uniform mat4 PVM)
        glm::mat4 PVM = PV * customSprites_[i].instance.model;
        glUniformMatrix4fv(glGetUniformLocation(currentShader, "PVM"),
                           1, GL_FALSE, glm::value_ptr(PVM));

        // Switch texture if needed (only for textured sprites)
        if (spriteTex != 0 && spriteTex != currentTex)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, spriteTex);
            currentTex = spriteTex;
        }

        // Apply material uniforms (shader-specific properties like colors)
        auto* mat = registry->Materials().Get(sprite->GetMaterial());
        if (mat)
            mat->ApplyUniforms(currentShader);

        glDrawElementsBaseVertex(
            GL_TRIANGLES,
            counts[i],
            GL_UNSIGNED_INT,
            offsets[i],
            baseVertices[i]);
    }

    // Restore the textured instanced shader for the next End() unbind
    if (currentShader != 0 && currentShader != instancedProgramId_)
    {
        auto* shader = registry->Shaders().Get(shaderHandle_);
        if (shader) shader->pipeline.Bind();
    }

    glBindVertexArray(0);
}

// --- Procedural mega-draw (single draw call for all procedural sprites) -----
// Packs geometry + per-vertex material params into one VBO, draws everything
// in a single glDrawElements call -- eliminates per-sprite uniform uploads.
//
// Vertex layout (14 floats per vertex, tightly packed struct):
//   worldPos(3), uv(2), localPos(2), baseColor(3), edgeColor(3)+edgeWidth(1)
//
// Positions are pre-transformed to world space on CPU; shader only applies PV.

void SpriteBatch::RenderProceduralMegaDraw()
{
    using Clock = std::chrono::high_resolution_clock;
    using Ms    = std::chrono::duration<float, std::milli>;

    if (proceduralProgramId_ == 0) {
        if (!customSprites_.empty()) RenderCustomSprites();
        return;
    }

    // Try compute shader path (GPU vertex transform) -- much faster for 500+ sprites
    if (!computeReady_) InitComputeShaders();
    if (computeReady_ && procSoA_.count >= 64)
    {
        RenderProceduralCompute();
        return;
    }

    static float elapsedTime = 0.f;
    elapsedTime += dt_;

    auto registry = GetDependency(AssetRegistry);
    const glm::mat4 PV = ctx_->Projection * ctx_->View;

    // --- CPU build phase (timed) -------------------------------------------
    auto buildStart = Clock::now();

    constexpr int kFloatsPerVert = 14;

    // Tightly packed vertex struct for bulk writes
    struct PackedVert {
        float wx, wy, wz;         // world pos
        float u, v;                // UV
        float lx, ly;             // local pos
        float br, bg, bb;         // base color
        float er, eg, eb, ew;     // edge color + width
    };
    static_assert(sizeof(PackedVert) == kFloatsPerVert * sizeof(float));

    const size_t spriteCount = procSoA_.count;

    // --- Phase 1 ELIMINATED: SoA data already gathered at Submit() time ---

    // --- Phase 2: Prefix sum for offsets (sequential, trivial) ------------
    // We still need vert/index counts from shape refs.
    struct OffsetInfo {
        size_t numVerts   = 0;
        size_t numIndices = 0;
        size_t vertOffset = 0;
        size_t idxOffset  = 0;
    };

    std::vector<OffsetInfo> offsets(spriteCount);
    size_t totalVerts  = 0;
    size_t totalIndices = 0;
    int triCount = 0;

    for (size_t i = 0; i < spriteCount; ++i)
    {
        const auto* shape = procSoA_.shapes[i].shape;
        offsets[i].numVerts   = shape->vertices.size();
        offsets[i].numIndices = shape->indices.size();
        offsets[i].vertOffset = totalVerts;
        offsets[i].idxOffset  = totalIndices;
        totalVerts  += offsets[i].numVerts;
        totalIndices += offsets[i].numIndices;
        triCount += static_cast<int>(offsets[i].numIndices) / 3;
    }

    if (totalVerts == 0) return;

    // Allocate exact-size staging buffers
    procStagingVerts_.resize(totalVerts * kFloatsPerVert);
    procStagingIndices_.resize(totalIndices);

    // --- Phase 3: Fill vertex/index buffers from SoA (parallel) -----------
    // All per-sprite data is now in contiguous arrays -- no pointer chasing.
    // SoA read pattern: procSoA_.m00[i], procSoA_.br[i], etc.
    // Shape vertex data: procSoA_.shapes[i].shape->vertices[vi]
    auto fillFn = [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i)
        {
            const auto& off = offsets[i];
            if (off.numVerts == 0) continue;

            const auto* shape = procSoA_.shapes[i].shape;

            // Read transform from SoA (contiguous floats -- SIMD-friendly)
            const float sm00 = procSoA_.m00[i], sm10 = procSoA_.m10[i], sm30 = procSoA_.m30[i];
            const float sm01 = procSoA_.m01[i], sm11 = procSoA_.m11[i], sm31 = procSoA_.m31[i];
            const float sm02 = procSoA_.m02[i], sm12 = procSoA_.m12[i], sm32 = procSoA_.m32[i];

            // Read material from SoA (contiguous)
            const float sbr = procSoA_.br[i], sbg = procSoA_.bg[i], sbb = procSoA_.bb[i];
            const float ser = procSoA_.er[i], seg = procSoA_.eg[i], seb = procSoA_.eb[i];
            const float sew = procSoA_.ew[i];

            // Write vertices at pre-computed offset
            PackedVert* dst = reinterpret_cast<PackedVert*>(
                procStagingVerts_.data() + off.vertOffset * kFloatsPerVert);

            for (size_t vi = 0; vi < off.numVerts; ++vi)
            {
                const auto& sv = shape->vertices[vi];
                const float px = sv.position.x, py = sv.position.y;
                dst[vi] = {
                    sm00 * px + sm10 * py + sm30,
                    sm01 * px + sm11 * py + sm31,
                    sm02 * px + sm12 * py + sm32,
                    sv.uv.x, sv.uv.y,
                    px, py,
                    sbr, sbg, sbb,
                    ser, seg, seb, sew
                };
            }

            // Write indices at pre-computed offset
            const unsigned int base = static_cast<unsigned int>(off.vertOffset);
            unsigned int* idxDst = procStagingIndices_.data() + off.idxOffset;
            for (size_t ii = 0; ii < off.numIndices; ++ii)
                idxDst[ii] = base + shape->indices[ii];
        }
    };

    if (threadRegistry_ && spriteCount >= 128)
        threadRegistry_->ParallelFor(spriteCount, fillFn, 64);
    else
        fillFn(0, spriteCount);

    stats_.cpuBuildMs = Ms(Clock::now() - buildStart).count();

    // --- GPU upload phase (timed) ------------------------------------------
    // Uses persistent mapped buffers (triple-buffered) when available.
    // Phase 3 wrote to CPU staging buffers; now we either:
    //   a) memcpy to the persistent map (zero driver overhead), or
    //   b) fall back to glBufferSubData if persistent mapping isn't supported.
    auto uploadStart = Clock::now();

    glBindVertexArray(procVAO_);

    const size_t vboBytes = procStagingVerts_.size() * sizeof(float);
    const size_t eboBytes = procStagingIndices_.size() * sizeof(unsigned int);

    // Try to set up persistent mapped buffers on first use or when capacity grows
    if (!procPersistentReady_ || vboBytes > procPersistentVBOCap_ || eboBytes > procPersistentEBOCap_)
    {
        // Tear down old persistent buffers if they exist
        if (procPersistentReady_)
        {
            // CRITICAL: wait for ALL in-flight frames to finish reading the old buffer
            // before deleting it. Without this, the GPU may still be reading from the
            // old buffer when we delete it → crash after enough fusions grow the buffer.
            for (int f = 0; f < kProcFrames; ++f)
            {
                if (procFences_[f])
                {
                    glClientWaitSync(procFences_[f], GL_SYNC_FLUSH_COMMANDS_BIT, 5000000000ULL);
                    glDeleteSync(procFences_[f]);
                    procFences_[f] = nullptr;
                }
            }
            glFinish(); // ensure all GPU commands complete before unmapping

            glBindBuffer(GL_ARRAY_BUFFER, procVBO_);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, procEBO_);
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            procMappedVBO_ = nullptr;
            procMappedEBO_ = nullptr;
            procPersistentReady_ = false;

            // Delete and recreate buffers (glBufferStorage can't be re-allocated)
            glDeleteBuffers(1, &procVBO_);
            glDeleteBuffers(1, &procEBO_);
            glGenBuffers(1, &procVBO_);
            glGenBuffers(1, &procEBO_);
        }

        // Allocate 3x capacity for triple-buffering, with 50% growth headroom
        const size_t vboCap = static_cast<size_t>(static_cast<double>(vboBytes) * 1.5);
        const size_t eboCap = static_cast<size_t>(static_cast<double>(eboBytes) * 1.5);
        const size_t totalVBOSize = vboCap * kProcFrames;
        const size_t totalEBOSize = eboCap * kProcFrames;

        constexpr GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

        glBindBuffer(GL_ARRAY_BUFFER, procVBO_);
        glBufferStorage(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(totalVBOSize), nullptr, mapFlags);
        procMappedVBO_ = reinterpret_cast<float*>(
            glMapBufferRange(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(totalVBOSize), mapFlags));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, procEBO_);
        glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(totalEBOSize), nullptr, mapFlags);
        procMappedEBO_ = reinterpret_cast<unsigned int*>(
            glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(totalEBOSize), mapFlags));

        if (procMappedVBO_ && procMappedEBO_)
        {
            procPersistentVBOCap_ = vboCap;
            procPersistentEBOCap_ = eboCap;
            procPersistentReady_ = true;
            procFrameIdx_ = 0;
        }
        else
        {
            // Persistent mapping not supported -- fall back to classic path
            if (procMappedVBO_) { glBindBuffer(GL_ARRAY_BUFFER, procVBO_); glUnmapBuffer(GL_ARRAY_BUFFER); }
            if (procMappedEBO_) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, procEBO_); glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER); }
            procMappedVBO_ = nullptr;
            procMappedEBO_ = nullptr;

            // Recreate as DYNAMIC_DRAW buffers
            glDeleteBuffers(1, &procVBO_);
            glDeleteBuffers(1, &procEBO_);
            glGenBuffers(1, &procVBO_);
            glGenBuffers(1, &procEBO_);
        }
    }

    if (procPersistentReady_)
    {
        // Wait for the fence on this frame slot (GPU done reading it)
        if (procFences_[procFrameIdx_])
        {
            glClientWaitSync(procFences_[procFrameIdx_], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
            glDeleteSync(procFences_[procFrameIdx_]);
            procFences_[procFrameIdx_] = nullptr;
        }

        // Write directly to the persistent map at this frame's offset
        const size_t vboOffset = procFrameIdx_ * procPersistentVBOCap_ / sizeof(float);
        const size_t eboOffset = procFrameIdx_ * procPersistentEBOCap_ / sizeof(unsigned int);

        std::memcpy(procMappedVBO_ + vboOffset, procStagingVerts_.data(), vboBytes);
        std::memcpy(procMappedEBO_ + eboOffset, procStagingIndices_.data(), eboBytes);

        // Set up vertex attribs pointing to this frame's region
        glBindBuffer(GL_ARRAY_BUFFER, procVBO_);
        const size_t vboByteOffset = static_cast<size_t>(procFrameIdx_) * procPersistentVBOCap_;

        SetupProceduralVertexAttribs(vboByteOffset);

        // EBO already bound, just set draw offset
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, procEBO_);
    }
    else
    {
        // Fallback: classic glBufferData/glBufferSubData
        glBindBuffer(GL_ARRAY_BUFFER, procVBO_);
        if (vboBytes > procVBOCapacity_)
        {
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vboBytes),
                         procStagingVerts_.data(), GL_DYNAMIC_DRAW);
            procVBOCapacity_ = vboBytes;
        }
        else
        {
            glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vboBytes),
                            procStagingVerts_.data());
        }

        SetupProceduralVertexAttribs();

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, procEBO_);
        if (eboBytes > procEBOCapacity_)
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(eboBytes),
                         procStagingIndices_.data(), GL_DYNAMIC_DRAW);
            procEBOCapacity_ = eboBytes;
        }
        else
        {
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                            static_cast<GLsizeiptr>(eboBytes), procStagingIndices_.data());
        }
    }

    stats_.cpuUploadMs = Ms(Clock::now() - uploadStart).count();

    // --- Draw (GPU-timed) --------------------------------------------------
    // Begin GPU timer query
    if (gpuTimerQuery_[0] == 0)
    {
        glGenQueries(2, gpuTimerQuery_);
        gpuTimerReady_ = false;
    }

    // Read previous frame's GPU time (1-frame latency)
    int prevIdx = 1 - gpuTimerFrame_;
    if (gpuTimerReady_)
    {
        GLuint64 gpuNs = 0;
        glGetQueryObjectui64v(gpuTimerQuery_[prevIdx], GL_QUERY_RESULT, &gpuNs);
        stats_.gpuMs = static_cast<float>(gpuNs) / 1e6f;
    }

    glBeginQuery(GL_TIME_ELAPSED, gpuTimerQuery_[gpuTimerFrame_]);

    glUseProgram(proceduralProgramId_);
    glUniformMatrix4fv(glGetUniformLocation(proceduralProgramId_, "uPV"),
                       1, GL_FALSE, glm::value_ptr(PV));
    glUniform1f(glGetUniformLocation(proceduralProgramId_, "time"), elapsedTime);

    // Draw with correct EBO offset for persistent mapping
    if (procPersistentReady_)
    {
        const size_t eboByteOffset = static_cast<size_t>(procFrameIdx_) * procPersistentEBOCap_;
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(procStagingIndices_.size()),
                       GL_UNSIGNED_INT, reinterpret_cast<void*>(eboByteOffset));
    }
    else
    {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(procStagingIndices_.size()),
                       GL_UNSIGNED_INT, nullptr);
    }

    glEndQuery(GL_TIME_ELAPSED);

    // Insert fence so we know when GPU is done reading this frame's data
    if (procPersistentReady_)
    {
        procFences_[procFrameIdx_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        procFrameIdx_ = (procFrameIdx_ + 1) % kProcFrames;
    }

    gpuTimerFrame_ = 1 - gpuTimerFrame_;
    gpuTimerReady_ = true;

    // Update stats
    stats_.drawCalls     = 1;
    stats_.vertexCount   = static_cast<int>(totalVerts);
    stats_.triangleCount = triCount;

    // Restore instanced shader
    auto* shader = registry->Shaders().Get(shaderHandle_);
    if (shader) shader->pipeline.Bind();

    glBindVertexArray(0);
}

// --- Instance VBO management ------------------------------------------------

void SpriteBatch::EnsureInstanceVBO(size_t count)
{
    size_t needed = count * sizeof(InstanceData);
    if (needed <= instanceVBOCapacity_) return;

    size_t newCap = std::max(needed, instanceVBOCapacity_ * 3 / 2);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(newCap), nullptr, GL_DYNAMIC_DRAW);
    instanceVBOCapacity_ = newCap;
}

void SpriteBatch::SetupInstanceAttributes(GLuint vao)
{
    glBindVertexArray(vao);

    GLuint iVBO = (vao == customVAO_) ? customInstanceVBO_ : instanceVBO_;
    glBindBuffer(GL_ARRAY_BUFFER, iVBO);

    const GLsizei stride = sizeof(InstanceData);

    for (int col = 0; col < 4; ++col)
    {
        GLuint loc = 2 + col;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, stride,
                              (void*)(offsetof(InstanceData, model) + col * sizeof(glm::vec4)));
        glVertexAttribDivisor(loc, 1);
    }

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(InstanceData, tiling));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
}

void SpriteBatch::SetupProceduralVertexAttribs(size_t byteOffset)
{
    constexpr int kFloatsPerVert = 14;
    constexpr GLsizei stride = kFloatsPerVert * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)(byteOffset));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(byteOffset + 3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(byteOffset + 5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(byteOffset + 7 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(byteOffset + 10 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(2, 0);
    glVertexAttribDivisor(3, 0);
    glVertexAttribDivisor(4, 0);
}

// --- Compute shader support (GL 4.5) ----------------------------------------

static GLuint CompileComputeShader(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        spdlog::warn("[SpriteBatch] Cannot open compute shader: {}", path);
        return 0;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* srcPtr = src.c_str();
    glShaderSource(shader, 1, &srcPtr, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        GLint len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        spdlog::error("[SpriteBatch] Compute shader compile error ({}): {}", path, log);
        glDeleteShader(shader);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader);
    glLinkProgram(prog);
    glDeleteShader(shader);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint len;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        spdlog::error("[SpriteBatch] Compute shader link error ({}): {}", path, log);
        glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

static void EnsureSSBO(GLuint& ssbo, size_t& cap, size_t needed)
{
    if (needed <= cap) return;
    size_t newCap = std::max(needed, cap * 3 / 2);
    if (ssbo == 0) glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(newCap), nullptr, GL_DYNAMIC_DRAW);
    cap = newCap;
}

void SpriteBatch::InitComputeShaders()
{
    if (computeReady_) return;

    // Check GL 4.5 support (compute shaders require it)
    if (!GLEW_VERSION_4_5)
    {
        spdlog::warn("[SpriteBatch] GL 4.5 not available -- compute shaders disabled");
        return;
    }

    // Resolve shader path
    auto resources = GetDependency(Globals);
    if (!resources)
    {
        spdlog::warn("[SpriteBatch] Globals not available -- compute shaders deferred");
        return;
    }
    std::string shadersPath = resources->GetWorkingFolder()
                            + resources->Get(gk::prefix::PATHS, gk::key::PATH_SHADERS);

    computeVertProg_ = CompileComputeShader(shadersPath + "sprite_transform.comp");
    computeIdxProg_  = CompileComputeShader(shadersPath + "sprite_index_transform.comp");

    if (computeVertProg_ && computeIdxProg_)
    {
        computeReady_ = true;
        spdlog::info("[SpriteBatch] Compute shaders ready (vert={}, idx={})",
                     computeVertProg_, computeIdxProg_);
    }
    else
    {
        spdlog::warn("[SpriteBatch] Compute shaders unavailable -- using CPU fallback");
    }
}

void SpriteBatch::RenderProceduralCompute()
{
    using Clock = std::chrono::high_resolution_clock;
    using Ms    = std::chrono::duration<float, std::milli>;

    auto registry = GetDependency(AssetRegistry);
    const glm::mat4 PV = ctx_->Projection * ctx_->View;

    static float elapsedTime = 0.f;
    elapsedTime += dt_;

    const size_t spriteCount = procSoA_.count;
    constexpr int kFloatsPerVert = 14;

    auto buildStart = Clock::now();

    // --- Build per-sprite info + source vertex/index buffers (CPU) ----------
    // GPU SpriteInfo layout matches the compute shader struct:
    //   9 floats (transform) + 7 floats (material) + 4 uints (offsets/counts) = 20 values
    constexpr int kInfoFloats = 16;  // 9 + 7
    constexpr int kInfoUints  = 4;   // dstVertOffset, dstIdxOffset, numVerts, numIndices
    constexpr int kInfoStride = kInfoFloats + kInfoUints; // 20 values per sprite

    // Phase 2: prefix sum (cheap, sequential)
    size_t totalVerts = 0, totalIndices = 0;
    int triCount = 0;
    std::vector<size_t> vertOffsets(spriteCount), idxOffsets(spriteCount);
    std::vector<size_t> vertCounts(spriteCount), idxCounts(spriteCount);

    for (size_t i = 0; i < spriteCount; ++i)
    {
        const auto* shape = procSoA_.shapes[i].shape;
        vertCounts[i]  = shape->vertices.size();
        idxCounts[i]   = shape->indices.size();
        vertOffsets[i] = totalVerts;
        idxOffsets[i]  = totalIndices;
        totalVerts  += vertCounts[i];
        totalIndices += idxCounts[i];
        triCount += static_cast<int>(idxCounts[i]) / 3;
    }

    if (totalVerts == 0) return;

    // Build SpriteInfo SSBO data
    std::vector<float> spriteInfoData(spriteCount * kInfoStride);
    for (size_t i = 0; i < spriteCount; ++i)
    {
        float* p = spriteInfoData.data() + i * kInfoStride;
        p[0]  = procSoA_.m00[i]; p[1]  = procSoA_.m10[i]; p[2]  = procSoA_.m30[i];
        p[3]  = procSoA_.m01[i]; p[4]  = procSoA_.m11[i]; p[5]  = procSoA_.m31[i];
        p[6]  = procSoA_.m02[i]; p[7]  = procSoA_.m12[i]; p[8]  = procSoA_.m32[i];
        p[9]  = procSoA_.br[i];  p[10] = procSoA_.bg[i];  p[11] = procSoA_.bb[i];
        p[12] = procSoA_.er[i];  p[13] = procSoA_.eg[i];  p[14] = procSoA_.eb[i];
        p[15] = procSoA_.ew[i];

        // Pack uints after floats (reinterpret memory)
        auto* u = reinterpret_cast<uint32_t*>(p + kInfoFloats);
        u[0] = static_cast<uint32_t>(vertOffsets[i]);
        u[1] = static_cast<uint32_t>(idxOffsets[i]);
        u[2] = static_cast<uint32_t>(vertCounts[i]);
        u[3] = static_cast<uint32_t>(idxCounts[i]);
    }

    // Build source vertex buffer (pos2 + uv2 = 4 floats per vert)
    std::vector<float> srcVerts(totalVerts * 4);
    std::vector<uint32_t> srcIndices(totalIndices);
    std::vector<uint32_t> vertToSprite(totalVerts);
    std::vector<uint32_t> idxToSprite(totalIndices);

    for (size_t i = 0; i < spriteCount; ++i)
    {
        const auto* shape = procSoA_.shapes[i].shape;
        float* vDst = srcVerts.data() + vertOffsets[i] * 4;
        for (size_t vi = 0; vi < vertCounts[i]; ++vi)
        {
            const auto& sv = shape->vertices[vi];
            vDst[vi * 4 + 0] = sv.position.x;
            vDst[vi * 4 + 1] = sv.position.y;
            vDst[vi * 4 + 2] = sv.uv.x;
            vDst[vi * 4 + 3] = sv.uv.y;
        }

        uint32_t* iDst = srcIndices.data() + idxOffsets[i];
        for (size_t ii = 0; ii < idxCounts[i]; ++ii)
            iDst[ii] = shape->indices[ii]; // local indices (base 0 per sprite)

        // Fill mapping buffers
        for (size_t vi = 0; vi < vertCounts[i]; ++vi)
            vertToSprite[vertOffsets[i] + vi] = static_cast<uint32_t>(i);
        for (size_t ii = 0; ii < idxCounts[i]; ++ii)
            idxToSprite[idxOffsets[i] + ii] = static_cast<uint32_t>(i);
    }

    stats_.cpuBuildMs = Ms(Clock::now() - buildStart).count();

    // --- Upload SSBOs -------------------------------------------------------
    auto uploadStart = Clock::now();

    const size_t infoBytes = spriteInfoData.size() * sizeof(float);
    const size_t srcVBytes = srcVerts.size() * sizeof(float);
    const size_t srcIBytes = srcIndices.size() * sizeof(uint32_t);
    const size_t v2sBytes  = vertToSprite.size() * sizeof(uint32_t);
    const size_t i2sBytes  = idxToSprite.size() * sizeof(uint32_t);
    const size_t dstVBytes = totalVerts * kFloatsPerVert * sizeof(float);
    const size_t dstIBytes = totalIndices * sizeof(uint32_t);

    EnsureSSBO(ssboSpriteInfo_,   ssboSpriteInfoCap_,   infoBytes);
    EnsureSSBO(ssboSrcVerts_,     ssboSrcVertsCap_,     srcVBytes);
    EnsureSSBO(ssboSrcIndices_,   ssboSrcIndicesCap_,   srcIBytes);
    EnsureSSBO(ssboVertToSprite_, ssboVertToSpriteCap_, v2sBytes);
    EnsureSSBO(ssboIdxToSprite_,  ssboIdxToSpriteCap_,  i2sBytes);
    EnsureSSBO(ssboDstVerts_,     ssboDstVertsCap_,     dstVBytes);
    EnsureSSBO(ssboDstIndices_,   ssboDstIndicesCap_,   dstIBytes);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSpriteInfo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(infoBytes), spriteInfoData.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSrcVerts_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(srcVBytes), srcVerts.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSrcIndices_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(srcIBytes), srcIndices.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboVertToSprite_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(v2sBytes), vertToSprite.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboIdxToSprite_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(i2sBytes), idxToSprite.data());

    stats_.cpuUploadMs = Ms(Clock::now() - uploadStart).count();

    // --- Dispatch vertex transform compute shader ---------------------------
    glUseProgram(computeVertProg_);
    glUniform1ui(glGetUniformLocation(computeVertProg_, "uTotalVerts"),
                 static_cast<GLuint>(totalVerts));

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboSpriteInfo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboSrcVerts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboDstVerts_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboVertToSprite_);

    glDispatchCompute(static_cast<GLuint>((totalVerts + 255) / 256), 1, 1);

    // --- Dispatch index transform compute shader ----------------------------
    glUseProgram(computeIdxProg_);
    glUniform1ui(glGetUniformLocation(computeIdxProg_, "uTotalIndices"),
                 static_cast<GLuint>(totalIndices));

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboSpriteInfo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboSrcIndices_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboDstIndices_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboIdxToSprite_);

    glDispatchCompute(static_cast<GLuint>((totalIndices + 255) / 256), 1, 1);

    // Memory barrier: ensure compute writes are visible to vertex fetch
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

    // --- Render using the compute-generated buffers -------------------------
    if (gpuTimerQuery_[0] == 0)
    {
        glGenQueries(2, gpuTimerQuery_);
        gpuTimerReady_ = false;
    }

    int prevIdx = 1 - gpuTimerFrame_;
    if (gpuTimerReady_)
    {
        GLuint64 gpuNs = 0;
        glGetQueryObjectui64v(gpuTimerQuery_[prevIdx], GL_QUERY_RESULT, &gpuNs);
        stats_.gpuMs = static_cast<float>(gpuNs) / 1e6f;
    }

    glBeginQuery(GL_TIME_ELAPSED, gpuTimerQuery_[gpuTimerFrame_]);

    // Bind the compute output SSBO as VBO/EBO for rendering
    glBindVertexArray(procVAO_);

    glBindBuffer(GL_ARRAY_BUFFER, ssboDstVerts_);
    SetupProceduralVertexAttribs();

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ssboDstIndices_);

    glUseProgram(proceduralProgramId_);
    glUniformMatrix4fv(glGetUniformLocation(proceduralProgramId_, "uPV"),
                       1, GL_FALSE, glm::value_ptr(PV));
    glUniform1f(glGetUniformLocation(proceduralProgramId_, "time"), elapsedTime);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(totalIndices),
                   GL_UNSIGNED_INT, nullptr);

    glEndQuery(GL_TIME_ELAPSED);
    gpuTimerFrame_ = 1 - gpuTimerFrame_;
    gpuTimerReady_ = true;

    stats_.drawCalls     = 1;
    stats_.vertexCount   = static_cast<int>(totalVerts);
    stats_.triangleCount = triCount;

    auto* shader = registry->Shaders().Get(shaderHandle_);
    if (shader) shader->pipeline.Bind();

    glBindVertexArray(0);
}

} // namespace ettycc
