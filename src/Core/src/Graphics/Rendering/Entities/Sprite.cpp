#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Scene/Assets/MaterialDef.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <UI/Widgets/PathFieldWidget.hpp>
#include <cmath>
#include <set>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace ettycc {
    Sprite::Sprite() {
        shaderName_ = kDefaultShader;
        initializable_ = true;
    }

    Sprite::Sprite(const std::string &spriteFilePath, bool initialize) : spriteFilePath_(spriteFilePath) {
        shaderName_ = kDefaultShader;
        initializable_ = initialize;
    }

    Sprite::Sprite(const std::string &spriteFilePath) : spriteFilePath_(spriteFilePath) {
        shaderName_ = kDefaultShader;
        initializable_ = true;
    }

    void Sprite::UploadGeometry()
    {
        auto vbuf = shape_.BuildVertexBuffer();
        indexCount_ = static_cast<int>(shape_.indices.size());

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vbuf.size() * sizeof(float)),
                     vbuf.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(shape_.indices.size() * sizeof(unsigned int)),
                     shape_.indices.data(), GL_DYNAMIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // TexCoord attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void Sprite::SetShape(const SpriteShape& shape)
    {
        shape_ = shape;
        if (VAO != 0) // already initialized — re-upload
            UploadGeometry();
    }

    auto Sprite::InitBackend(const std::string &texturePath, const std::string& shaderName) -> void {
        // Generate GL objects
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // Upload shape geometry
        UploadGeometry();

        // ── Shader (shared via ResourceCache) ────────────────────────────────
        auto cache = GetDependency(ResourceCache);
        const std::string& shader = shaderName.empty() ? std::string(kDefaultShader) : shaderName;
        cachedShader_ = cache->GetShader(shader);

        // ── Texture (shared via ResourceCache) ───────────────────────────────
        TEXTURE = cache->GetTexture(texturePath);

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

    void Sprite::ResolveMaterial()
    {
        if (materialPath_.empty()) return;

        auto globals = GetDependency(Globals);
        if (!globals) return;

        std::string absMatPath = globals->GetWorkingFolder() + materialPath_;
        std::ifstream ifs(absMatPath);
        if (!ifs.is_open())
        {
            spdlog::warn("[Sprite] Cannot open material: {}", absMatPath);
            return;
        }

        try {
            nlohmann::json j;
            ifs >> j;
            auto mat = MaterialDef::FromJson(j);
            if (!mat.texture.empty())
                spriteFilePath_ = mat.texture;
            if (!mat.shader.empty())
                shaderName_ = mat.shader;
            spdlog::info("[Sprite] Resolved material '{}' -> shader: {}, texture: {}",
                         materialPath_, shaderName_, spriteFilePath_);
        } catch (const std::exception& e) {
            spdlog::error("[Sprite] Material parse error '{}': {}", absMatPath, e.what());
        }
    }

    // Renderable
    void Sprite::Init(const std::shared_ptr<Engine> &engineCtx) {
        if (initialized || !initializable_)
            return;

        // Resolve material to texture + shader if a material path is set
        ResolveMaterial();

        if (spriteFilePath_.empty())
            return;

        auto engineResources = GetDependency(Globals);
        const auto fullPath = engineResources->GetWorkingFolder() + spriteFilePath_;

        spdlog::info("Initializing sprite [{}]", fullPath);
        InitBackend(fullPath, shaderName_);

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

        glm::mat4 ProjectionViewMatrix = ctx->Projection * ctx->View * underylingTransform.GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(prog, "PVM"), 1, GL_FALSE,
                           glm::value_ptr(ProjectionViewMatrix));

        glUniform1f(glGetUniformLocation(prog, "deltaTime"), time);
        glUniform1f(glGetUniformLocation(prog, "time"), elapsedTime);

        const glm::vec3 scale = underylingTransform.getGlobalScale();
        const glm::vec2 tiling(std::abs(scale.x) * tilingMultiplier_,
                               std::abs(scale.y) * tilingMultiplier_);
        glUniform2f(glGetUniformLocation(prog, "tiling"),
                    tiling.x, tiling.y);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, 0);

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

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    GLuint Sprite::GetShaderProgramId() const {
        return cachedShader_ ? cachedShader_->programId : 0;
    }

    void Sprite::Inspect(EditorPropertyVisitor &v) {
        Renderable::Inspect(v); // Enabled + Transform section
        PROP_SECTION("Sprite");

        // ── Material slot (drag-drop target) ─────────────────────────────
        {
            ++v.propertyCount;
            const float avail      = ImGui::GetContentRegionAvail().x;
            const float labelWidth = ImMax(80.f, avail * 0.38f);
            const float widgetWidth = avail - labelWidth - ImGui::GetStyle().ItemSpacing.x;

            ImGui::PushID("Material");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Material");
            ImGui::SameLine(labelWidth);

            // Show current material name (or "None") as a selectable drop target
            std::string displayName = materialPath_.empty() ? "None (drop material here)" :
                std::filesystem::path(materialPath_).filename().string();

            ImGui::SetNextItemWidth(widgetWidth);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, materialPath_.empty()
                ? ImVec4(0.15f, 0.15f, 0.15f, 1.f)
                : ImVec4(0.2f, 0.15f, 0.05f, 1.f));
            ImGui::InputText("##matslot", displayName.data(), displayName.size(),
                             ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor();

            // Drag-drop target: accept MATERIAL_ASSET payload
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_ASSET"))
                {
                    std::string droppedPath(static_cast<const char*>(payload->Data),
                                            payload->DataSize - 1);
                    materialPath_ = droppedPath;
                    v.anyChanged = true;
                    spdlog::info("[Sprite] Material assigned: {}", materialPath_);
                }
                ImGui::EndDragDropTarget();
            }

            // Clear button
            if (!materialPath_.empty())
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("x##clrmat"))
                {
                    materialPath_.clear();
                    v.anyChanged = true;
                }
            }

            ImGui::PopID();
        }

        // Show resolved paths as read-only info
        PROP_RO(spriteFilePath_, "Texture");
        PROP_RO(shaderName_, "Shader");
        PROP(tilingMultiplier_, "Tiling Multiplier");

        // Show current shape info (read-only)
        PROP_SECTION("Shape");
        PROP(shape_.name, "Shape Type");
        int vertCount = static_cast<int>(shape_.vertices.size());
        v.Property("Vertices", vertCount, PROP_READ_ONLY);
        int triCount = static_cast<int>(shape_.indices.size() / 3);
        v.Property("Triangles", triCount, PROP_READ_ONLY);
    }
}
