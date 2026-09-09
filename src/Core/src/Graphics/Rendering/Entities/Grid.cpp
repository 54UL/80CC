#include <Engine.hpp>
#include <Graphics/Rendering/Entities/Grid.hpp>
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Scene/Assets/AssetRegistry.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    Grid::Grid() { renderableType = Type::Grid; }

    Grid::~Grid()
    {
        glDeleteVertexArrays(1, &VAO_);
        glDeleteBuffers(1, &VBO_);
    }

    void Grid::Init(const std::shared_ptr<Engine>& /*engineCtx*/)
    {
        if (initialized) return;

        // -- Shader via AssetRegistry ------------------------------------------
        auto registry = GetDependency(AssetRegistry);
        shaderHandle_ = registry->GetShader(kShaderName);
        auto* shader = registry->Shaders().Get(shaderHandle_);

        if (shader)
        {
            const GLuint prog = shader->programId;
            pvmLoc_      = glGetUniformLocation(prog, "PVM");
            modelLoc_    = glGetUniformLocation(prog, "model");
            camPosLoc_   = glGetUniformLocation(prog, "camPos");
            viewSizeLoc_ = glGetUniformLocation(prog, "viewSize");
        }

        BuildGeometry();
        initialized = true;
    }

    void Grid::BuildGeometry()
    {
        constexpr float H = 100.0f;
        const float verts[] = {
            -H, -H, 0.0f,
             H, -H, 0.0f,
             H,  H, 0.0f,
            -H, -H, 0.0f,
             H,  H, 0.0f,
            -H,  H, 0.0f,
        };

        glGenVertexArrays(1, &VAO_);
        glGenBuffers(1, &VBO_);

        glBindVertexArray(VAO_);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }

    void Grid::Pass(const std::shared_ptr<RenderingContext>& ctx, float /*deltaTime*/)
    {
        auto registry = GetDependency(AssetRegistry);
        auto* shader = registry->Shaders().Get(shaderHandle_);
        if (!shader) return;

        glm::vec3 camWorld = glm::vec3(glm::inverse(ctx->View)[3]);

        const float projHalfH = (ctx->Projection[1][1] != 0.f)
                               ? 1.0f / ctx->Projection[1][1]
                               : 5.0f;
        const float viewSize = projHalfH * 3.0f;
        const float quadScale = glm::max(1.0f, viewSize / 100.0f);

        glm::mat4 model = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(glm::floor(camWorld.x), glm::floor(camWorld.y), 0.0f));
        model = glm::scale(model, glm::vec3(quadScale, quadScale, 1.0f));

        glm::mat4 PVM = ctx->Projection * ctx->View * model;

        shader->pipeline.Bind();
        glUniformMatrix4fv(pvmLoc_,   1, GL_FALSE, glm::value_ptr(PVM));
        glUniformMatrix4fv(modelLoc_, 1, GL_FALSE, glm::value_ptr(model));
        glUniform2f(camPosLoc_, camWorld.x, camWorld.y);
        glUniform1f(viewSizeLoc_, viewSize);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        glBindVertexArray(VAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        shader->pipeline.Unbind();
    }
} // namespace ettycc
