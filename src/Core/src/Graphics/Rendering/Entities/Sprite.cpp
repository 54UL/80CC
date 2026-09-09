#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Scene/Assets/AssetRegistry.hpp>
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
        renderableType = Type::Sprite;
        shaderName_ = kDefaultShader;
        initializable_ = true;
    }

    Sprite::Sprite(const std::string &spriteFilePath, bool initialize) : spriteFilePath_(spriteFilePath) {
        renderableType = Type::Sprite;
        shaderName_ = kDefaultShader;
        initializable_ = initialize;
    }

    Sprite::Sprite(const std::string &spriteFilePath) : spriteFilePath_(spriteFilePath) {
        renderableType = Type::Sprite;
        shaderName_ = kDefaultShader;
        initializable_ = true;
    }

    std::shared_ptr<Sprite> Sprite::MakeWithShader(const SpriteShape& shape,
                                                      const std::string& shaderName,
                                                      const std::unordered_map<std::string, UniformValue>& uniforms)
    {
        auto sprite = std::make_shared<Sprite>();
        sprite->procedural_ = true;
        sprite->shape_ = shape;
        sprite->indexCount_ = static_cast<int>(shape.indices.size());
        sprite->shaderName_ = shaderName;
        sprite->initializable_ = true;
        // Store uniforms so ResolveMaterial() can create the auto-material
        sprite->pendingUniforms_ = uniforms;
        return sprite;
    }

    std::shared_ptr<Sprite> Sprite::MakeProcedural(const SpriteShape& shape,
                                                     const ProceduralRockParams& params)
    {
        return MakeWithShader(shape, "rock_procedural", params.ToUniforms());
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
        if (VAO != 0) // already initialized -- re-upload
            UploadGeometry();
    }

    void Sprite::InitBackend() {
        // Generate GL objects for per-sprite geometry
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // Upload shape geometry
        UploadGeometry();

        // Resolve material -> ensure texture and shader are loaded to GPU
        auto registry = GetDependency(AssetRegistry);
        auto* mat = registry->Materials().Get(materialHandle_);
        if (!mat) return;

        // Ensure texture is loaded (triggers GPU upload if not yet done)
        if (!mat->texturePath.empty())
            mat->texture = registry->GetTexture(mat->texturePath);
        if (!mat->shaderName.empty())
            mat->shader = registry->GetShader(mat->shaderName);

        // Set the texture sampler uniform once (harmless for shaders that don't use it)
        auto* shader = registry->Shaders().Get(mat->shader);
        if (shader)
        {
            shader->pipeline.Bind();
            glUniform1i(glGetUniformLocation(shader->programId, "ourTexture"), 0);
            shader->pipeline.Unbind();
        }
    }

    Sprite::~Sprite() {
        // Clean up geometry only -- shader and texture are owned by AssetRegistry
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    bool Sprite::IsTextureless() const
    {
        auto registry = GetDependency(AssetRegistry);
        auto* mat = registry ? registry->Materials().Get(materialHandle_) : nullptr;
        if (!mat) return procedural_; // fallback to legacy flag
        return mat->texturePath.empty();
    }

    void Sprite::ResolveMaterial()
    {
        auto registry = GetDependency(AssetRegistry);
        if (!registry) return;

        // Textureless / procedural path: create an auto-material from shaderName_
        // and any pending uniforms (from MakeWithShader) or legacy ProceduralRockParams.
        if (procedural_ && !shaderName_.empty() && shaderName_ != kDefaultShader)
        {
            std::string autoKey = "auto::procedural::" + std::to_string(reinterpret_cast<uintptr_t>(this));
            materialHandle_ = registry->Materials().Find(autoKey);
            if (!materialHandle_.IsValid())
            {
                MaterialAsset mat;
                mat.shaderName = shaderName_;
                mat.shader = registry->GetShader(mat.shaderName);
                // Prefer pending uniforms (from MakeWithShader), fall back to legacy params
                mat.uniforms = pendingUniforms_.empty()
                    ? proceduralParams_.ToUniforms()
                    : pendingUniforms_;
                materialHandle_ = registry->Materials().Add(autoKey, std::move(mat));
            }
            pendingUniforms_.clear();
            return;
        }

        if (!materialPath_.empty())
        {
            // Explicit .material file -- load/resolve it
            materialHandle_ = registry->GetMaterial(materialPath_);
            auto* mat = registry->Materials().Get(materialHandle_);
            if (mat)
            {
                // Sync the serialized paths from the material
                if (!mat->texturePath.empty())
                    spriteFilePath_ = mat->texturePath;
                if (!mat->shaderName.empty())
                    shaderName_ = mat->shaderName;

                registry->ResolveMaterialHandles(materialHandle_);
                spdlog::info("[Sprite] Resolved material '{}' -> shader: {}, texture: {}",
                             materialPath_, shaderName_, spriteFilePath_);
                return;
            }
        }

        // No explicit material -- create/get an auto-material from the texture path
        if (!spriteFilePath_.empty())
        {
            materialHandle_ = registry->GetOrCreateDefaultMaterial(spriteFilePath_);
            auto* mat = registry->Materials().Get(materialHandle_);
            if (mat && !shaderName_.empty() && shaderName_ != kDefaultShader)
            {
                // Override shader if the sprite had a custom one serialized
                mat->shaderName = shaderName_;
                mat->shader = registry->GetShader(shaderName_);
            }
        }
    }

    // Renderable
    void Sprite::Init(const std::shared_ptr<Engine> &engineCtx) {
        if (initialized || !initializable_)
            return;

        // Resolve material handle from serialized paths (handles both textured and procedural)
        ResolveMaterial();

        if (!materialHandle_.IsValid())
        {
            // No material could be resolved -- nothing to render
            if (spriteFilePath_.empty() && materialPath_.empty() && !procedural_)
                return;

            spdlog::warn("[Sprite] Could not resolve material (tex: '{}', mat: '{}', shader: '{}')",
                         spriteFilePath_, materialPath_, shaderName_);
            return;
        }

        spdlog::info("Initializing sprite [material: {}, shader: {}, texture: {}]",
                     materialPath_.empty() ? "(auto)" : materialPath_,
                     shaderName_, spriteFilePath_);
        InitBackend();

        // Apply pixelsPerUnit from texture asset to set proper sprite size
        auto registry = GetDependency(AssetRegistry);
        auto* mat = registry->Materials().Get(materialHandle_);
        if (mat)
        {
            auto* tex = registry->Textures().Get(mat->texture);
            if (tex && tex->pixelsPerUnit > 0.f && tex->sourceWidth > 0 && tex->sourceHeight > 0)
            {
                float w = static_cast<float>(tex->sourceWidth)  / tex->pixelsPerUnit;
                float h = static_cast<float>(tex->sourceHeight) / tex->pixelsPerUnit;
                auto& t = transform();
                glm::vec3 scale = t.getGlobalScale();
                if (glm::abs(scale.x - 1.f) < 0.001f && glm::abs(scale.y - 1.f) < 0.001f)
                {
                    t.setGlobalScale({w * 0.5f, h * 0.5f, scale.z});
                }
            }
        }

        initialized = true;
    }

    void Sprite::Pass(const std::shared_ptr<RenderingContext> &ctx, float time) {
        static float elapsedTime = 0;
        elapsedTime += time;

        // Unified rendering path: material drives everything
        auto registry = GetDependency(AssetRegistry);
        auto* mat = registry->Materials().Get(materialHandle_);
        if (!mat) return;

        auto* shader = registry->Shaders().Get(mat->shader);
        if (!shader) return;

        const GLuint prog = shader->programId;
        const bool hasTexture = !mat->texturePath.empty() && mat->texture.IsValid();

        // Bind texture if the material has one
        if (hasTexture)
        {
            GLuint texHandle = registry->GetGLTextureHandle(mat->texture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texHandle);
        }

        shader->pipeline.Bind();

        // Engine-managed uniforms
        glm::mat4 PVM = ctx->Projection * ctx->View * transform().GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(prog, "PVM"), 1, GL_FALSE,
                           glm::value_ptr(PVM));
        glUniform1f(glGetUniformLocation(prog, "deltaTime"), time);
        glUniform1f(glGetUniformLocation(prog, "time"), elapsedTime);

        // Tiling (only relevant for textured shaders)
        if (hasTexture)
        {
            glm::vec2 tiling(1.0f, 1.0f);
            if (GetTextureWrapMode() != TextureWrapMode::FitToShape)
            {
                const glm::vec3 scale = transform().getGlobalScale();
                tiling = glm::vec2(glm::abs(scale.x) * tilingMultiplier_,
                                   glm::abs(scale.y) * tilingMultiplier_);
            }
            glUniform2f(glGetUniformLocation(prog, "tiling"), tiling.x, tiling.y);
        }

        // Apply material's custom uniforms (shader-specific properties)
        mat->ApplyUniforms(prog);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, 0);

        shader->pipeline.Unbind();
        glBindVertexArray(0);

        if (hasTexture)
            glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Sprite::DrawForPicker(const std::shared_ptr<RenderingContext> &ctx,
                               GLuint program, uint32_t id) {
        if (!initialized) return;

        glm::mat4 PVM = ctx->Projection * ctx->View * transform().GetMatrix();
        glUniformMatrix4fv(glGetUniformLocation(program, "PVM"), 1, GL_FALSE,
                           glm::value_ptr(PVM));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    GLuint Sprite::GetShaderProgramId() const {
        auto registry = GetDependency(AssetRegistry);
        auto* mat = registry->Materials().Get(materialHandle_);
        if (!mat) return 0;
        return registry->GetGLShaderProgram(mat->shader);
    }

    GLuint Sprite::GetTextureHandle() const {
        auto registry = GetDependency(AssetRegistry);
        auto* mat = registry->Materials().Get(materialHandle_);
        if (!mat) return 0;
        if (mat->texturePath.empty()) return 0;
        return registry->GetGLTextureHandle(mat->texture);
    }

    TextureWrapMode Sprite::GetTextureWrapMode() const {
        auto registry = GetDependency(AssetRegistry);
        auto* mat = registry ? registry->Materials().Get(materialHandle_) : nullptr;
        auto* tex = (mat) ? registry->Textures().Get(mat->texture) : nullptr;
        return tex ? tex->wrapMode : TextureWrapMode::Repeat;
    }

    void Sprite::Inspect(EditorPropertyVisitor &v) {
        Renderable::Inspect(v); // Enabled + Transform section
        PROP_SECTION("Sprite");

        // -- Material slot (drag-drop target) -----------------------------
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

                    // Re-resolve material handle
                    auto registry = GetDependency(AssetRegistry);
                    if (registry)
                    {
                        materialHandle_ = registry->GetMaterial(materialPath_);
                        registry->ResolveMaterialHandles(materialHandle_);
                        auto* mat = registry->Materials().Get(materialHandle_);
                        if (mat)
                        {
                            spriteFilePath_ = mat->texturePath;
                            shaderName_     = mat->shaderName;
                        }
                    }

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
                    // Revert to auto-material
                    auto registry = GetDependency(AssetRegistry);
                    if (registry && !spriteFilePath_.empty())
                        materialHandle_ = registry->GetOrCreateDefaultMaterial(spriteFilePath_);
                    v.anyChanged = true;
                }
            }

            ImGui::PopID();
        }

        // Show resolved paths as read-only info
        PROP_RO(spriteFilePath_, "Texture");
        PROP_RO(shaderName_, "Shader");
        PROP(tilingMultiplier_, "Tiling Multiplier");

        // -- Texture asset info (read-only -- edit via asset inspector) --------
        {
            auto registry = GetDependency(AssetRegistry);
            auto* mat = registry ? registry->Materials().Get(materialHandle_) : nullptr;
            auto* tex = (registry && mat) ? registry->Textures().Get(mat->texture) : nullptr;
            if (tex)
            {
                PROP_SECTION("Image Info");
                static const char* filterNames[] = { "Point", "Bilinear", "Trilinear" };
                static const char* wrapNames[]   = { "Repeat", "Clamp", "Mirror Repeat", "Fit to Shape" };
                ImGui::TextDisabled("Size: %d x %d  |  Filter: %s  |  PPU: %.0f",
                    tex->sourceWidth, tex->sourceHeight,
                    filterNames[static_cast<int>(tex->filterMode)],
                    tex->pixelsPerUnit);

                int wrapIdx = static_cast<int>(tex->wrapMode);
                if (ImGui::Combo("Wrap Mode", &wrapIdx, wrapNames, IM_ARRAYSIZE(wrapNames)))
                {
                    tex->wrapMode = static_cast<TextureWrapMode>(wrapIdx);
                    tex->ApplyGLParams();
                    registry->SaveTextureMeta(mat->texture);
                    v.anyChanged = true;
                }
            }
        }

        // -- Shader uniform properties (driven by introspection) -----------
        {
            auto registry = GetDependency(AssetRegistry);
            auto* mat = registry ? registry->Materials().Get(materialHandle_) : nullptr;
            if (mat)
            {
                auto* shader = registry->Shaders().Get(mat->shader);
                if (shader && !shader->userUniforms.empty())
                {
                    PROP_SECTION("Shader Properties");
                    ImGui::TextDisabled("Shader: %s", mat->shaderName.c_str());

                    for (const auto& uInfo : shader->userUniforms)
                    {
                        ImGui::PushID(uInfo.name.c_str());

                        // Check if the material has a value for this uniform
                        auto it = mat->uniforms.find(uInfo.name);

                        switch (uInfo.type)
                        {
                        case GL_FLOAT:
                        {
                            float val = 0.f;
                            if (it != mat->uniforms.end() && std::holds_alternative<float>(it->second))
                                val = std::get<float>(it->second);
                            if (ImGui::DragFloat(uInfo.name.c_str(), &val, 0.01f))
                            {
                                mat->uniforms[uInfo.name] = val;
                                v.anyChanged = true;
                            }
                            break;
                        }
                        case GL_FLOAT_VEC2:
                        {
                            glm::vec2 val(0.f);
                            if (it != mat->uniforms.end() && std::holds_alternative<glm::vec2>(it->second))
                                val = std::get<glm::vec2>(it->second);
                            if (ImGui::DragFloat2(uInfo.name.c_str(), &val[0], 0.01f))
                            {
                                mat->uniforms[uInfo.name] = val;
                                v.anyChanged = true;
                            }
                            break;
                        }
                        case GL_FLOAT_VEC3:
                        {
                            glm::vec3 val(0.f);
                            if (it != mat->uniforms.end() && std::holds_alternative<glm::vec3>(it->second))
                                val = std::get<glm::vec3>(it->second);
                            // Use ColorEdit3 for uniforms with "Color" in the name
                            bool isColor = (uInfo.name.find("Color") != std::string::npos
                                         || uInfo.name.find("color") != std::string::npos);
                            if (isColor)
                            {
                                if (ImGui::ColorEdit3(uInfo.name.c_str(), &val[0]))
                                {
                                    mat->uniforms[uInfo.name] = val;
                                    v.anyChanged = true;
                                }
                            }
                            else
                            {
                                if (ImGui::DragFloat3(uInfo.name.c_str(), &val[0], 0.01f))
                                {
                                    mat->uniforms[uInfo.name] = val;
                                    v.anyChanged = true;
                                }
                            }
                            break;
                        }
                        case GL_FLOAT_VEC4:
                        {
                            glm::vec4 val(0.f);
                            if (it != mat->uniforms.end() && std::holds_alternative<glm::vec4>(it->second))
                                val = std::get<glm::vec4>(it->second);
                            bool isColor = (uInfo.name.find("Color") != std::string::npos
                                         || uInfo.name.find("color") != std::string::npos);
                            if (isColor)
                            {
                                if (ImGui::ColorEdit4(uInfo.name.c_str(), &val[0]))
                                {
                                    mat->uniforms[uInfo.name] = val;
                                    v.anyChanged = true;
                                }
                            }
                            else
                            {
                                if (ImGui::DragFloat4(uInfo.name.c_str(), &val[0], 0.01f))
                                {
                                    mat->uniforms[uInfo.name] = val;
                                    v.anyChanged = true;
                                }
                            }
                            break;
                        }
                        case GL_INT:
                        case GL_BOOL:
                        {
                            int val = 0;
                            if (it != mat->uniforms.end() && std::holds_alternative<int>(it->second))
                                val = std::get<int>(it->second);
                            if (uInfo.type == GL_BOOL)
                            {
                                bool b = (val != 0);
                                if (ImGui::Checkbox(uInfo.name.c_str(), &b))
                                {
                                    mat->uniforms[uInfo.name] = b ? 1 : 0;
                                    v.anyChanged = true;
                                }
                            }
                            else
                            {
                                if (ImGui::DragInt(uInfo.name.c_str(), &val))
                                {
                                    mat->uniforms[uInfo.name] = val;
                                    v.anyChanged = true;
                                }
                            }
                            break;
                        }
                        default:
                            ImGui::TextDisabled("%s (%s)", uInfo.name.c_str(),
                                               ShaderAsset::GLTypeName(uInfo.type));
                            break;
                        }

                        ImGui::PopID();
                    }
                }
            }
        }

        // Show current shape info (read-only)
        PROP_SECTION("Shape");
        PROP(shape_.name, "Shape Type");
        int vertCount = static_cast<int>(shape_.vertices.size());
        v.Property("Vertices", vertCount, PROP_READ_ONLY);
        int triCount = static_cast<int>(shape_.indices.size() / 3);
        v.Property("Triangles", triCount, PROP_READ_ONLY);
    }
}
