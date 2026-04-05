#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace ettycc {
    Sprite::Sprite() {
        initializable_ = true;
    }

    Sprite::Sprite(const std::string &spriteFilePath, bool initialize) : spriteFilePath_(spriteFilePath) {
        initializable_ = initialize;
    }

    Sprite::Sprite(const std::string &spriteFilePath) : spriteFilePath_(spriteFilePath) {
        initializable_ = true;
    }

    auto Sprite::InitBackend(const std::string &spritePath) -> void {
        // Quad vertices and texture coordinates
        float vertices[]{
            // Positions         // Texture Coords
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f
        };

        // Indices to form two triangles for the quad
        unsigned int indices[]{
            0, 1, 2,
            2, 3, 0
        };

        // Generate and bind a Vertex Array Object (VAO)
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // Bind the VAO
        glBindVertexArray(VAO);

        // Bind and set vertex buffer data
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Bind and set element buffer data
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // Set vertex attribute pointers
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Unbind the VAO (not strictly necessary but advised)
        glBindVertexArray(0);

        // ── Shader (shared via ResourceCache) ────────────────────────────────
        auto cache = GetDependency(ResourceCache);
        cachedShader_ = cache->GetShader(kShaderName);

        // ── Texture (shared via ResourceCache) ───────────────────────────────
        TEXTURE = cache->GetTexture(spritePath);

        // Set the texture sampler uniform once
        if (cachedShader_)
        {
            cachedShader_->pipeline.Bind();
            glUniform1i(glGetUniformLocation(cachedShader_->programId, "ourTexture"), 0);
            cachedShader_->pipeline.Unbind();
        }
    }

    Sprite::~Sprite() {
        // Clean up geometry only — shader and texture are owned by ResourceCache
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    // Renderable
    void Sprite::Init(const std::shared_ptr<Engine> &engineCtx) {
        if (initialized || !initializable_ || spriteFilePath_.empty())
            return;

        auto engineResources = GetDependency(Globals);
        const auto fullPath = engineResources->GetWorkingFolder() + spriteFilePath_;

        spdlog::info("Initializing sprite [{}]", fullPath);
        InitBackend(fullPath);

        initialized = true;
    }

    void Sprite::Pass(const std::shared_ptr<RenderingContext> &ctx, float time) {
        if (!cachedShader_) return;

        static float elapsedTime = 0;
        elapsedTime += time;

        const GLuint prog = cachedShader_->programId;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, TEXTURE);

        cachedShader_->pipeline.Bind();

        // Multiply projection * view * model matrix to compute sprite rendering...
        glm::mat4 ProjectionViewMatrix = ctx->Projection * ctx->View * underylingTransform.GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(prog, "PVM"), 1, GL_FALSE,
                           glm::value_ptr(ProjectionViewMatrix));

        glUniform1f(glGetUniformLocation(prog, "deltaTime"), time);
        glUniform1f(glGetUniformLocation(prog, "time"), elapsedTime);

        // Scale-relative tiling: texture density stays constant regardless of stretch.
        // tiling = scale.xy * multiplier, so a 5x1 sprite tiles 5x in X by default.
        const glm::vec3 scale = underylingTransform.getGlobalScale();
        const glm::vec2 tiling(std::abs(scale.x) * tilingMultiplier_,
                               std::abs(scale.y) * tilingMultiplier_);
        glUniform2f(glGetUniformLocation(prog, "tiling"),
                    tiling.x, tiling.y);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        cachedShader_->pipeline.Unbind();
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Sprite::DrawForPicker(const std::shared_ptr<RenderingContext> &ctx,
                               GLuint program, uint32_t id) {
        if (!initialized) return;

        glm::mat4 PVM = ctx->Projection * ctx->View * underylingTransform.GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(program, "PVM"), 1, GL_FALSE,
                           glm::value_ptr(PVM));

        // idColor is already set by RenderPass (golden-ratio hue)
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    GLuint Sprite::GetShaderProgramId() const {
        return cachedShader_ ? cachedShader_->programId : 0;
    }

    void Sprite::Inspect(EditorPropertyVisitor &v) {
        Renderable::Inspect(v); // Enabled + Transform section
        PROP_SECTION("Sprite");
        PROP(spriteFilePath_, "Texture Path");
        PROP(tilingMultiplier_, "Tiling Multiplier");
    }
}
