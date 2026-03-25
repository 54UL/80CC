#include <Graphics/Rendering/Entities/SoftBodyRenderable.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <Engine.hpp>

// stb_image is already implemented in Sprite.cpp — include header only here
#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>

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
        glDeleteVertexArrays(1, &VAO_);
        glDeleteBuffers(1, &VBO_);
        glDeleteBuffers(1, &EBO_);
        glDeleteTextures(1, &TEXTURE_);
    }

    // -------------------------------------------------------------------------
    // Init
    // -------------------------------------------------------------------------
    void SoftBodyRenderable::Init(const std::shared_ptr<Engine>& /*engineCtx*/)
    {
        if (initialized)
            return;

        LoadShaders();
        LoadTexture();

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
        if (!initialized || !body_)
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

        // Soft body nodes are already in world space — no model matrix needed
        glm::mat4 PV = ctx->Projection * ctx->View;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, TEXTURE_);

        shader_.Bind();
        glUniformMatrix4fv(glGetUniformLocation(shader_.GetProgramId(), "PV"),
                           1, GL_FALSE, glm::value_ptr(PV));

        glBindVertexArray(VAO_);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices_.size()),
                       GL_UNSIGNED_INT, 0);

        shader_.Unbind();
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
    }

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------
    std::string SoftBodyRenderable::LoadShaderFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            spdlog::error("[SoftBodyRenderable] failed to open shader file {}", path);
            return {};
        }
        std::stringstream buf;
        buf << file.rdbuf();
        return buf.str();
    }

    void SoftBodyRenderable::LoadShaders()
    {
        auto resources   = GetDependency(Resources);
        auto shadersPath = resources->GetWorkingFolder() + "/" + resources->Get("paths", "shaders");

        auto vert = LoadShaderFile(shadersPath + shaderBaseName_ + ".vert");
        auto frag = LoadShaderFile(shadersPath + shaderBaseName_ + ".frag");

        std::vector<std::shared_ptr<Shader>> shaders = {
            std::make_shared<Shader>(vert.c_str(), GL_VERTEX_SHADER),
            std::make_shared<Shader>(frag.c_str(), GL_FRAGMENT_SHADER)
        };
        shader_.AddShaders(shaders);
        shader_.Create();
    }

    void SoftBodyRenderable::LoadTexture()
    {
        auto resources = GetDependency(Resources);
        const std::string fullPath = resources->GetWorkingFolder() + "\\" + texturePath_;

        glGenTextures(1, &TEXTURE_);
        glBindTexture(GL_TEXTURE_2D, TEXTURE_);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, numChannels;
        unsigned char* image = stbi_load(fullPath.c_str(), &width, &height, &numChannels, 0);
        if (image)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, image);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(image);
        }
        else
        {
            spdlog::error("[SoftBodyRenderable] Error loading image [{}]: {}",
                          fullPath, stbi_failure_reason());
        }

        shader_.Bind();
        glUniform1i(glGetUniformLocation(shader_.GetProgramId(), "ourTexture"), 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // namespace ettycc
