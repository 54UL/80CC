#include <Graphics/Rendering/Entities/SoftBodyRenderable.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <Engine.hpp>

// stb_image is already implemented in Sprite.cpp — include header only here
#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    SoftBodyRenderable::SoftBodyRenderable(std::string         texPath,
                                           btSoftBody*         body,
                                           std::vector<float>  uvs,
                                           std::vector<int>    indices)
        : texturePath_(std::move(texPath))
        , body_(body)
        , indices_(std::move(indices))
    {
        numVerts_ = static_cast<int>(body_->m_nodes.size());

        // Build interleaved vertex buffer: x,y,z,u,v per vertex
        // Positions will be overwritten each frame from bullet nodes;
        // UVs are stored permanently at construction time.
        vertexBuffer_.resize(numVerts_ * 5, 0.0f);
        for (int i = 0; i < numVerts_; ++i)
        {
            // UV comes from the caller-supplied flat array (u0,v0, u1,v1, ...)
            vertexBuffer_[i * 5 + 3] = (i * 2 + 0 < static_cast<int>(uvs.size())) ? uvs[i * 2 + 0] : 0.0f;
            vertexBuffer_[i * 5 + 4] = (i * 2 + 1 < static_cast<int>(uvs.size())) ? uvs[i * 2 + 1] : 0.0f;
        }
    }

    SoftBodyRenderable::~SoftBodyRenderable()
    {
        // Geometry is owned per-instance; shader+texture are owned by ResourceCache
        glDeleteVertexArrays(1, &VAO_);
        glDeleteBuffers(1, &VBO_);
        glDeleteBuffers(1, &EBO_);
    }

    // -------------------------------------------------------------------------
    // Init
    // -------------------------------------------------------------------------
    void SoftBodyRenderable::Init(const std::shared_ptr<Engine>& /*engineCtx*/)
    {
        if (initialized)
            return;

        // ── Shader + Texture via ResourceCache ───────────────────────────────
        auto cache = GetDependency(ResourceCache);
        cachedShader_ = cache->GetShader(kShaderName);

        auto resources = GetDependency(Globals);
        const std::string fullPath = resources->GetWorkingFolder() + texturePath_;
        TEXTURE_ = cache->GetTexture(fullPath);

        // Set sampler uniform once
        if (cachedShader_)
        {
            cachedShader_->pipeline.Bind();
            glUniform1i(glGetUniformLocation(cachedShader_->programId, "ourTexture"), 0);
            cachedShader_->pipeline.Unbind();
        }

        // Seed positions from the initial bullet node positions
        for (int i = 0; i < numVerts_; ++i)
        {
            const btVector3& p = body_->m_nodes[i].m_x;
            vertexBuffer_[i * 5 + 0] = static_cast<float>(p.x());
            vertexBuffer_[i * 5 + 1] = static_cast<float>(p.y());
            vertexBuffer_[i * 5 + 2] = static_cast<float>(p.z());
        }

        // VAO / VBO / EBO
        glGenVertexArrays(1, &VAO_);
        glGenBuffers(1, &VBO_);
        glGenBuffers(1, &EBO_);

        glBindVertexArray(VAO_);

        // VBO — dynamic because positions are updated every frame
        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertexBuffer_.size() * sizeof(float)),
                     vertexBuffer_.data(),
                     GL_DYNAMIC_DRAW);

        // EBO — static; topology never changes
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices_.size() * sizeof(int)),
                     indices_.data(),
                     GL_STATIC_DRAW);

        // attrib 0 — position (vec3, offset 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              5 * static_cast<GLsizei>(sizeof(float)),
                              reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);

        // attrib 1 — uv (vec2, offset 12 bytes)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                              5 * static_cast<GLsizei>(sizeof(float)),
                              reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        initialized = true;
        spdlog::info("[SoftBodyRenderable] initialized — {} verts, {} indices",
                     numVerts_, indices_.size());
    }

    // -------------------------------------------------------------------------
    // Pass
    // -------------------------------------------------------------------------
    void SoftBodyRenderable::Pass(const std::shared_ptr<RenderingContext>& ctx, float /*deltaTime*/)
    {
        if (!initialized || !body_ || !cachedShader_)
            return;

        // Pull updated world-space positions from Bullet
        for (int i = 0; i < numVerts_; ++i)
        {
            const btVector3& p = body_->m_nodes[i].m_x;
            vertexBuffer_[i * 5 + 0] = static_cast<float>(p.x());
            vertexBuffer_[i * 5 + 1] = static_cast<float>(p.y());
            vertexBuffer_[i * 5 + 2] = static_cast<float>(p.z());
        }

        // Upload only the interleaved position+uv data (positions changed, uvs unchanged)
        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(vertexBuffer_.size() * sizeof(float)),
                        vertexBuffer_.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        const GLuint prog = cachedShader_->programId;

        // Soft body nodes are already in world space — no model matrix needed
        glm::mat4 PV = ctx->Projection * ctx->View;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, TEXTURE_);

        cachedShader_->pipeline.Bind();
        glUniformMatrix4fv(glGetUniformLocation(prog, "PV"),
                           1, GL_FALSE, glm::value_ptr(PV));
        glUniform2f(glGetUniformLocation(prog, "tiling"),
                    tiling.x, tiling.y);

        glBindVertexArray(VAO_);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices_.size()),
                       GL_UNSIGNED_INT, 0);

        cachedShader_->pipeline.Unbind();
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // -------------------------------------------------------------------------
    // DrawForPicker
    // -------------------------------------------------------------------------
    void SoftBodyRenderable::DrawForPicker(const std::shared_ptr<RenderingContext>& ctx,
                                           GLuint program, uint32_t /*id*/)
    {
        if (!initialized || !body_) return;

        // Soft body nodes are world-space — no model matrix, just PV
        const glm::mat4 PV = ctx->Projection * ctx->View;
        glUniformMatrix4fv(glGetUniformLocation(program, "PVM"), 1, GL_FALSE,
                           glm::value_ptr(PV));

        glBindVertexArray(VAO_);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices_.size()),
                       GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // -------------------------------------------------------------------------
    // Inspect
    // -------------------------------------------------------------------------
    void SoftBodyRenderable::Inspect(EditorPropertyVisitor& v)
    {
        Renderable::Inspect(v);
        PROP_SECTION("Soft Body");
        PROP_RO(numVerts_, "Vertices");
        PROP(tiling.x, "Tiling X");
        PROP(tiling.y, "Tiling Y");
    }

} // namespace ettycc
