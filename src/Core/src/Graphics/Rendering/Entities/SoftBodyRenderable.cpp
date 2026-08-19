#include <Graphics/Rendering/Entities/SoftBodyRenderable.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <Engine.hpp>

// stb_image is already implemented in Sprite.cpp -- include header only here
#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    SoftBodyRenderable::SoftBodyRenderable(std::string                texPath,
                                           physics::IPhysicsSoftBody* body,
                                           std::vector<float>         uvs,
                                           std::vector<int>           indices)
        : texturePath_(std::move(texPath))
        , body_(body)
        , indices_(std::move(indices))
    {
        numVerts_ = body_->GetNodeCount();

        vertexBuffer_.resize(numVerts_ * 5, 0.0f);
        for (int i = 0; i < numVerts_; ++i)
        {
            vertexBuffer_[i * 5 + 3] = (i * 2 + 0 < static_cast<int>(uvs.size())) ? uvs[i * 2 + 0] : 0.0f;
            vertexBuffer_[i * 5 + 4] = (i * 2 + 1 < static_cast<int>(uvs.size())) ? uvs[i * 2 + 1] : 0.0f;
        }
    }

    SoftBodyRenderable::~SoftBodyRenderable()
    {
        glDeleteVertexArrays(1, &VAO_);
        glDeleteBuffers(1, &VBO_);
        glDeleteBuffers(1, &EBO_);
    }

    void SoftBodyRenderable::Init(const std::shared_ptr<Engine>& /*engineCtx*/)
    {
        if (initialized)
            return;

        auto cache = GetDependency(ResourceCache);
        cachedShader_ = cache->GetShader(kShaderName);

        auto resources = GetDependency(Globals);
        const std::string fullPath = resources->GetWorkingFolder() + texturePath_;
        TEXTURE_ = cache->GetTexture(fullPath);

        if (cachedShader_)
        {
            cachedShader_->pipeline.Bind();
            glUniform1i(glGetUniformLocation(cachedShader_->programId, "ourTexture"), 0);
            cachedShader_->pipeline.Unbind();
        }

        // Seed positions from the initial node positions
        for (int i = 0; i < numVerts_; ++i)
        {
            const glm::vec3 p = body_->GetNodePosition(i);
            vertexBuffer_[i * 5 + 0] = p.x;
            vertexBuffer_[i * 5 + 1] = p.y;
            vertexBuffer_[i * 5 + 2] = p.z;
        }

        glGenVertexArrays(1, &VAO_);
        glGenBuffers(1, &VBO_);
        glGenBuffers(1, &EBO_);

        glBindVertexArray(VAO_);

        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertexBuffer_.size() * sizeof(float)),
                     vertexBuffer_.data(),
                     GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices_.size() * sizeof(int)),
                     indices_.data(),
                     GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              5 * static_cast<GLsizei>(sizeof(float)),
                              reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                              5 * static_cast<GLsizei>(sizeof(float)),
                              reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        initialized = true;
        spdlog::info("[SoftBodyRenderable] initialized -- {} verts, {} indices",
                     numVerts_, indices_.size());
    }

    void SoftBodyRenderable::Pass(const std::shared_ptr<RenderingContext>& ctx, float /*deltaTime*/)
    {
        if (!initialized || !body_ || !cachedShader_)
            return;

        // Pull updated world-space positions from physics
        for (int i = 0; i < numVerts_; ++i)
        {
            const glm::vec3 p = body_->GetNodePosition(i);
            vertexBuffer_[i * 5 + 0] = p.x;
            vertexBuffer_[i * 5 + 1] = p.y;
            vertexBuffer_[i * 5 + 2] = p.z;
        }

        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(vertexBuffer_.size() * sizeof(float)),
                        vertexBuffer_.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        const GLuint prog = cachedShader_->programId;

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

    void SoftBodyRenderable::DrawForPicker(const std::shared_ptr<RenderingContext>& ctx,
                                           GLuint program, uint32_t /*id*/)
    {
        if (!initialized || !body_) return;

        const glm::mat4 PV = ctx->Projection * ctx->View;
        glUniformMatrix4fv(glGetUniformLocation(program, "PVM"), 1, GL_FALSE,
                           glm::value_ptr(PV));

        glBindVertexArray(VAO_);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices_.size()),
                       GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void SoftBodyRenderable::Inspect(EditorPropertyVisitor& v)
    {
        Renderable::Inspect(v);
        PROP_SECTION("Soft Body");
        PROP_RO(numVerts_, "Vertices");
        PROP(tiling.x, "Tiling X");
        PROP(tiling.y, "Tiling Y");
    }

} // namespace ettycc
