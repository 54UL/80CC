#include <Engine.hpp>
#include <Graphics/Rendering/Entities/Grid.hpp>
#include <Graphics/Shading/Shader.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Dependency.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace ettycc
{
    Grid::Grid() = default;

    Grid::~Grid()
    {
        glDeleteVertexArrays(1, &VAO_);
        glDeleteBuffers(1, &VBO_);
    }

    void Grid::Init(const std::shared_ptr<Engine>& /*engineCtx*/)
    {
        if (initialized) return;

        auto resources   = GetDependency(Globals);
        auto shadersPath = resources->GetWorkingFolder() + "/" + resources->Get(gk::prefix::PATHS, gk::key::PATH_SHADERS);

        auto vert = LoadShaderFile(shadersPath + "grid.vert");
        auto frag = LoadShaderFile(shadersPath + "grid.frag");

        shader_.AddShaders({
            std::make_shared<Shader>(vert.c_str(), GL_VERTEX_SHADER),
            std::make_shared<Shader>(frag.c_str(), GL_FRAGMENT_SHADER)
        });
        shader_.Create();

        pvmLoc_    = glGetUniformLocation(shader_.GetProgramId(), "PVM");
        modelLoc_  = glGetUniformLocation(shader_.GetProgramId(), "model");
        camPosLoc_ = glGetUniformLocation(shader_.GetProgramId(), "camPos");

        BuildGeometry();
        initialized = true;
    }

    void Grid::BuildGeometry()
    {
        // Large flat quad in the XY plane. The fragment shader computes grid lines
        // analytically from world position, so only positions are needed.
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
        // Extract camera world-space XY from the inverse view matrix
        glm::vec3 camWorld = glm::vec3(glm::inverse(ctx->View)[3]);

        // Snap the quad to integer grid units so it always covers the camera
        // while grid lines stay fixed in world space (no swimming)
        glm::mat4 model = glm::translate(
            glm::mat4(1.0f),
            glm::vec3(glm::floor(camWorld.x), glm::floor(camWorld.y), 0.0f));

        glm::mat4 PVM = ctx->Projection * ctx->View * model;

        shader_.Bind();
        glUniformMatrix4fv(pvmLoc_,   1, GL_FALSE, glm::value_ptr(PVM));
        glUniformMatrix4fv(modelLoc_, 1, GL_FALSE, glm::value_ptr(model));
        glUniform2f(camPosLoc_, camWorld.x, camWorld.y);

        // Alpha blending for the glow/fade effect
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(VAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glDisable(GL_BLEND);
        shader_.Unbind();
    }

    std::string Grid::LoadShaderFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            spdlog::error("[Grid] Failed to open shader: {}", path);
            return {};
        }
        std::stringstream buf;
        buf << file.rdbuf();
        return buf.str();
    }
} // namespace ettycc
