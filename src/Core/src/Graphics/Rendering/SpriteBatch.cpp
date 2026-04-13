#include <Graphics/Rendering/SpriteBatch.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Graphics/Rendering/Entities/SpriteShape.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>
#include <Scene/Assets/ResourceCache.hpp>
#include <Dependency.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>

namespace ettycc
{

// ─── Lifecycle ──────────────────────────────────────────────────────────────

SpriteBatch::SpriteBatch()  = default;
SpriteBatch::~SpriteBatch()
{
    if (instanceVBO_)       glDeleteBuffers(1, &instanceVBO_);
    if (customVAO_)         glDeleteVertexArrays(1, &customVAO_);
    if (customVBO_)         glDeleteBuffers(1, &customVBO_);
    if (customEBO_)         glDeleteBuffers(1, &customEBO_);
    if (customInstanceVBO_) glDeleteBuffers(1, &customInstanceVBO_);

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

    // Load the instanced shader via ResourceCache
    auto cache = GetDependency(ResourceCache);
    instancedShader_ = cache->GetShader("sprite_instanced");
    if (!instancedShader_)
    {
        spdlog::error("[SpriteBatch] Failed to load sprite_instanced shader");
        return;
    }

    // Set texture sampler uniform once
    instancedShader_->pipeline.Bind();
    glUniform1i(glGetUniformLocation(instancedShader_->programId, "ourTexture"), 0);
    instancedShader_->pipeline.Unbind();

    // Create shared instance VBO (will grow as needed)
    glGenBuffers(1, &instanceVBO_);

    // Create custom geometry mega-buffer resources
    glGenVertexArrays(1, &customVAO_);
    glGenBuffers(1, &customVBO_);
    glGenBuffers(1, &customEBO_);
    glGenBuffers(1, &customInstanceVBO_);

    initialized_ = true;
    spdlog::info("[SpriteBatch] Initialized (instanced shader program {})",
                 instancedShader_->programId);
}

// ─── Per-frame API ──────────────────────────────────────────────────────────

void SpriteBatch::Begin(const std::shared_ptr<RenderingContext>& ctx, float dt)
{
    ctx_ = ctx;
    dt_  = dt;
    batches_.clear();
    customSprites_.clear();
}

void SpriteBatch::Submit(Sprite* sprite)
{
    if (!sprite || !sprite->initialized) return;

    const SpriteShape& shape = sprite->GetShape();
    const bool isCustom = (shape.preset == SpriteShape::Preset::Custom);

    // Build per-instance data
    InstanceData inst{};
    inst.model  = sprite->underylingTransform.GetMatrix();

    const glm::vec3 scale = sprite->underylingTransform.getGlobalScale();
    inst.tiling = glm::vec2(std::abs(scale.x) * sprite->GetTilingMultiplier(),
                            std::abs(scale.y) * sprite->GetTilingMultiplier());

    if (isCustom)
    {
        customSprites_.push_back({ sprite, inst });
    }
    else
    {
        BatchKey key{};
        key.shader      = instancedShader_->programId; // all instanced sprites use this shader
        key.texture     = sprite->GetTextureHandle();
        key.shapePreset = static_cast<int>(shape.preset);

        auto& batch     = batches_[key];
        batch.texture   = key.texture;
        batch.instances.push_back(inst);
    }
}

void SpriteBatch::End()
{
    if (!initialized_ || !instancedShader_) return;

    const glm::mat4 PV = ctx_->Projection * ctx_->View;
    const GLuint prog   = instancedShader_->programId;

    instancedShader_->pipeline.Bind();
    glUniformMatrix4fv(glGetUniformLocation(prog, "uPV"), 1, GL_FALSE, glm::value_ptr(PV));

    // ── 1. Instanced path: one draw call per (texture, shape) group ─────────
    for (auto& [key, batch] : batches_)
    {
        if (batch.instances.empty()) continue;

        PresetGeo& geo = GetPresetGeo(key.shapePreset);

        // Upload instance data
        EnsureInstanceVBO(batch.instances.size());
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(batch.instances.size() * sizeof(InstanceData)),
                        batch.instances.data());

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.texture);

        // Bind VAO (has geometry + instance attribs set up)
        glBindVertexArray(geo.vao);

        // Draw all instances in one call
        glDrawElementsInstanced(GL_TRIANGLES, geo.indexCount, GL_UNSIGNED_INT,
                                nullptr, static_cast<GLsizei>(batch.instances.size()));

        glBindVertexArray(0);
    }

    // ── 2. Custom geometry path ─────────────────────────────────────────────
    if (!customSprites_.empty())
        RenderCustomSprites();

    instancedShader_->pipeline.Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);

    ctx_.reset();
}

// ─── Preset geometry ────────────────────────────────────────────────────────

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

    // Geometry VBO (static — shared by all instances of this preset)
    glBindBuffer(GL_ARRAY_BUFFER, geo.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vbuf.size() * sizeof(float)),
                 vbuf.data(), GL_STATIC_DRAW);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(shape.indices.size() * sizeof(unsigned int)),
                 shape.indices.data(), GL_STATIC_DRAW);

    // Vertex attribs: location 0 = position (vec3), location 1 = texcoord (vec2)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Attach instance attributes to this VAO
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

// ─── Custom geometry (mega-buffer) ──────────────────────────────────────────

void SpriteBatch::RenderCustomSprites()
{
    // Sort by texture to minimize binds
    std::sort(customSprites_.begin(), customSprites_.end(),
              [](const CustomEntry& a, const CustomEntry& b)
              {
                  return a.sprite->GetTextureHandle() < b.sprite->GetTextureHandle();
              });

    // Pack all custom vertices and indices into mega-buffers
    std::vector<float>        megaVerts;
    std::vector<unsigned int> megaIndices;
    std::vector<InstanceData> megaInstances;

    // For glMultiDrawElementsBaseVertex
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
        megaInstances.push_back(entry.instance);

        counts.push_back(static_cast<GLsizei>(shape.indices.size()));
        offsets.push_back(reinterpret_cast<void*>(indexOffset * sizeof(unsigned int)));
        baseVertices.push_back(static_cast<GLint>(vertexOffset));

        vertexOffset += shape.vertices.size();
        indexOffset  += shape.indices.size();
    }

    // Upload mega vertex buffer
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

    // Vertex attribs
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Upload mega index buffer
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

    // For custom sprites we can't use true instancing (each has different geometry),
    // so we draw one at a time with per-sprite instance data via uniform fallback,
    // OR we use glMultiDrawElementsBaseVertex with a per-draw instance attribute trick.
    //
    // Simpler approach: iterate per texture group, set instance data for each draw.
    // This is still far fewer state changes than the old per-sprite path since we
    // share the VAO, shader, and mega-buffer.

    GLuint currentTex = 0;

    for (size_t i = 0; i < customSprites_.size(); ++i)
    {
        GLuint tex = customSprites_[i].sprite->GetTextureHandle();
        if (tex != currentTex)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            currentTex = tex;
        }

        // Upload single instance data for this draw
        glBindBuffer(GL_ARRAY_BUFFER, customInstanceVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData),
                     &megaInstances[i], GL_DYNAMIC_DRAW);

        // Set up instance attributes on the custom VAO
        SetupInstanceAttributes(customVAO_);

        glDrawElementsBaseVertex(
            GL_TRIANGLES,
            counts[i],
            GL_UNSIGNED_INT,
            offsets[i],
            baseVertices[i]);
    }

    glBindVertexArray(0);
}

// ─── Instance VBO management ────────────────────────────────────────────────

void SpriteBatch::EnsureInstanceVBO(size_t count)
{
    size_t needed = count * sizeof(InstanceData);
    if (needed <= instanceVBOCapacity_) return;

    // Grow by 1.5x to avoid frequent reallocs
    size_t newCap = std::max(needed, instanceVBOCapacity_ * 3 / 2);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(newCap), nullptr, GL_DYNAMIC_DRAW);
    instanceVBOCapacity_ = newCap;
}

void SpriteBatch::SetupInstanceAttributes(GLuint vao)
{
    glBindVertexArray(vao);

    // Bind the instance VBO that's currently relevant
    // (caller must have bound the right one before or we use instanceVBO_)
    // For preset geos we use instanceVBO_, for custom we use customInstanceVBO_.
    // We detect which by checking the vao.
    GLuint iVBO = (vao == customVAO_) ? customInstanceVBO_ : instanceVBO_;
    glBindBuffer(GL_ARRAY_BUFFER, iVBO);

    const GLsizei stride = sizeof(InstanceData);

    // mat4 model → locations 2, 3, 4, 5 (one vec4 per column)
    for (int col = 0; col < 4; ++col)
    {
        GLuint loc = 2 + col;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, stride,
                              (void*)(offsetof(InstanceData, model) + col * sizeof(glm::vec4)));
        glVertexAttribDivisor(loc, 1);
    }

    // vec2 tiling → location 6
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(InstanceData, tiling));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
}

} // namespace ettycc
