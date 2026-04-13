#include <Graphics/Rendering/PickerBuffer.hpp>
#include <Graphics/Rendering/Renderable.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Graphics/Rendering/Entities/SpriteShape.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace ettycc
{
    PickerBuffer::PickerBuffer(glm::ivec2 size) : size_(size) {}

    PickerBuffer::~PickerBuffer()
    {
        CleanUp();

        if (instanceVBO_) glDeleteBuffers(1, &instanceVBO_);

        for (auto& [preset, geo] : presetGeos_)
        {
            if (geo.vao) glDeleteVertexArrays(1, &geo.vao);
            if (geo.vbo) glDeleteBuffers(1, &geo.vbo);
            if (geo.ebo) glDeleteBuffers(1, &geo.ebo);
        }

        if (instancedProgram_) glDeleteProgram(instancedProgram_);
    }

    // -------------------------------------------------------------------------

    void PickerBuffer::Init()
    {
        if (size_.x <= 0 || size_.y <= 0) return;

        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

        // RGBA8 color attachment — displayable as ImGui::Image
        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size_.x, size_.y, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, texture_, 0);

        // Depth renderbuffer so depth testing works correctly
        glGenRenderbuffers(1, &depthRbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size_.x, size_.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, depthRbo_);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            spdlog::error("[PickerBuffer] Framebuffer not complete");
        else
            initialized_ = true;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    void PickerBuffer::InitShader(const std::string& shadersPath)
    {
        // ── Legacy per-object picker shader ─────────────────────────────────
        {
            auto vertSrc = LoadFile(shadersPath + "picker.vert");
            auto fragSrc = LoadFile(shadersPath + "picker.frag");
            if (vertSrc.empty() || fragSrc.empty()) return;

            GLuint vert = CompileShader(vertSrc, GL_VERTEX_SHADER);
            GLuint frag = CompileShader(fragSrc, GL_FRAGMENT_SHADER);

            program_ = glCreateProgram();
            glAttachShader(program_, vert);
            glAttachShader(program_, frag);
            glLinkProgram(program_);

            GLint ok = 0;
            glGetProgramiv(program_, GL_LINK_STATUS, &ok);
            if (!ok) {
                GLchar log[512];
                glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
                spdlog::error("[PickerBuffer] Shader link error: {}", log);
            }

            glDeleteShader(vert);
            glDeleteShader(frag);

            pvmLoc_     = glGetUniformLocation(program_, "PVM");
            idColorLoc_ = glGetUniformLocation(program_, "idColor");
        }

        // ── Instanced picker shader ─────────────────────────────────────────
        {
            auto vertSrc = LoadFile(shadersPath + "picker_instanced.vert");
            auto fragSrc = LoadFile(shadersPath + "picker_instanced.frag");
            if (vertSrc.empty() || fragSrc.empty())
            {
                spdlog::warn("[PickerBuffer] Instanced picker shaders not found — falling back to per-object path");
                return;
            }

            GLuint vert = CompileShader(vertSrc, GL_VERTEX_SHADER);
            GLuint frag = CompileShader(fragSrc, GL_FRAGMENT_SHADER);

            instancedProgram_ = glCreateProgram();
            glAttachShader(instancedProgram_, vert);
            glAttachShader(instancedProgram_, frag);
            glLinkProgram(instancedProgram_);

            GLint ok = 0;
            glGetProgramiv(instancedProgram_, GL_LINK_STATUS, &ok);
            if (!ok) {
                GLchar log[512];
                glGetProgramInfoLog(instancedProgram_, sizeof(log), nullptr, log);
                spdlog::error("[PickerBuffer] Instanced shader link error: {}", log);
                glDeleteProgram(instancedProgram_);
                instancedProgram_ = 0;
                glDeleteShader(vert);
                glDeleteShader(frag);
                return;
            }

            glDeleteShader(vert);
            glDeleteShader(frag);

            instancedPVLoc_ = glGetUniformLocation(instancedProgram_, "uPV");

            // Create shared instance VBO
            glGenBuffers(1, &instanceVBO_);

            spdlog::info("[PickerBuffer] Instanced picker shader ready (program {})",
                         instancedProgram_);
        }
    }

    // -------------------------------------------------------------------------

    void PickerBuffer::RenderPass(
        const std::shared_ptr<RenderingContext>& ctx,
        const std::vector<std::shared_ptr<Renderable>>& renderables)
    {
        if (!initialized_ || program_ == 0) return;

        colorToId_.clear();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, size_.x, size_.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);  // no blending — solid flat IDs only

        // ── Separate sprites from non-sprites and assign IDs ────────────────
        // Batches keyed by shape preset
        std::unordered_map<int, PickerBatch> batches;
        std::vector<CustomPickerEntry> customSprites;

        const bool canBatch = (instancedProgram_ != 0);

        for (uint32_t i = 0; i < static_cast<uint32_t>(renderables.size()); ++i)
        {
            const uint32_t  id  = i + 1;
            const glm::vec3 col = IdToColor(id);

            // Store quantised RGB -> id in lookup table
            const auto r8 = static_cast<uint8_t>(col.r * 255.0f + 0.5f);
            const auto g8 = static_cast<uint8_t>(col.g * 255.0f + 0.5f);
            const auto b8 = static_cast<uint8_t>(col.b * 255.0f + 0.5f);
            colorToId_[PackColor(r8, g8, b8)] = id;

            auto* sprite = dynamic_cast<Sprite*>(renderables[i].get());

            // Frustum culling — skip sprites entirely outside the view
            if (sprite && sprite->initialized && ctx->frustum.enabled)
            {
                const glm::vec3 pos   = sprite->underylingTransform.getGlobalPosition();
                const glm::vec3 scale = sprite->underylingTransform.getGlobalScale();
                if (!ctx->frustum.IsVisible(pos, scale.x, scale.y))
                    continue;
            }

            if (sprite && sprite->initialized && canBatch)
            {
                PickerInstanceData inst{};
                inst.model   = sprite->underylingTransform.GetMatrix();
                inst.idColor = col;
                inst._pad    = 0.f;

                const SpriteShape& shape = sprite->GetShape();
                const bool isCustom = (shape.preset == SpriteShape::Preset::Custom);

                if (isCustom)
                {
                    customSprites.push_back({ sprite, inst });
                }
                else
                {
                    int presetKey = static_cast<int>(shape.preset);
                    auto& batch   = batches[presetKey];
                    batch.shapePreset = presetKey;
                    batch.instances.push_back(inst);
                }
            }
            else
            {
                // Non-sprite or fallback: use legacy per-object path
                glUseProgram(program_);
                glUniform3f(idColorLoc_, col.r, col.g, col.b);
                renderables[i]->DrawForPicker(ctx, program_, id);
            }
        }

        // ── Instanced path: one draw call per shape preset ──────────────────
        if (canBatch && !batches.empty())
        {
            const glm::mat4 PV = ctx->Projection * ctx->View;

            glUseProgram(instancedProgram_);
            glUniformMatrix4fv(instancedPVLoc_, 1, GL_FALSE, glm::value_ptr(PV));

            for (auto& [presetKey, batch] : batches)
            {
                if (batch.instances.empty()) continue;

                PresetGeo& geo = GetPresetGeo(presetKey);

                // Upload instance data
                EnsureInstanceVBO(batch.instances.size());
                glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
                glBufferSubData(GL_ARRAY_BUFFER, 0,
                                static_cast<GLsizeiptr>(batch.instances.size() * sizeof(PickerInstanceData)),
                                batch.instances.data());

                glBindVertexArray(geo.vao);

                glDrawElementsInstanced(GL_TRIANGLES, geo.indexCount, GL_UNSIGNED_INT,
                                        nullptr, static_cast<GLsizei>(batch.instances.size()));

                glBindVertexArray(0);
            }
        }

        // ── Custom geometry sprites: individual draws with instanced shader ─
        if (canBatch && !customSprites.empty())
        {
            const glm::mat4 PV = ctx->Projection * ctx->View;

            glUseProgram(instancedProgram_);
            glUniformMatrix4fv(instancedPVLoc_, 1, GL_FALSE, glm::value_ptr(PV));

            for (auto& entry : customSprites)
            {
                if (!entry.sprite || !entry.sprite->initialized) continue;

                // Fall back to legacy per-object shader for custom geometry
                glUseProgram(program_);
                glUniform3f(idColorLoc_, entry.instance.idColor.r,
                            entry.instance.idColor.g, entry.instance.idColor.b);
                // Use the sprite's own VAO since custom geometry can't share preset geo
                glm::mat4 PVM = ctx->Projection * ctx->View * entry.instance.model;
                glUniformMatrix4fv(pvmLoc_, 1, GL_FALSE, glm::value_ptr(PVM));

                entry.sprite->DrawForPicker(ctx, program_, 0);
            }
        }

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    uint32_t PickerBuffer::ReadPixel(int x, int y) const
    {
        if (!initialized_) return 0;

        // Flip Y — OpenGL origin is bottom-left, screen origin is top-left
        const int flippedY = size_.y - 1 - y;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
        uint8_t pixel[4] = {0, 0, 0, 0};
        glReadPixels(x, flippedY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        // Background is pure black
        if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) return 0;

        const auto it = colorToId_.find(PackColor(pixel[0], pixel[1], pixel[2]));
        return (it != colorToId_.end()) ? it->second : 0;
    }

    // -------------------------------------------------------------------------

    void PickerBuffer::CleanUp()
    {
        if (fbo_)      { glDeleteFramebuffers(1,  &fbo_);      fbo_      = 0; }
        if (texture_)  { glDeleteTextures(1,      &texture_);  texture_  = 0; }
        if (depthRbo_) { glDeleteRenderbuffers(1, &depthRbo_); depthRbo_ = 0; }
        initialized_ = false;
    }

    void PickerBuffer::SetSize(glm::ivec2 size)
    {
        if (size.x <= 0 || size.y <= 0 || size == size_) return;
        size_ = size;
        CleanUp();
        Init();
    }

    GLuint PickerBuffer::GetTextureId() const { return texture_; }

    // ─── Preset geometry ────────────────────────────────────────────────────

    void PickerBuffer::BuildPresetGeo(int preset)
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
        // Only position needed for picker (no tex coords), but we keep the same
        // vertex layout (5 floats: x,y,z,u,v) for simplicity — only location 0 is read.
        glBindBuffer(GL_ARRAY_BUFFER, geo.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vbuf.size() * sizeof(float)),
                     vbuf.data(), GL_STATIC_DRAW);

        // EBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(shape.indices.size() * sizeof(unsigned int)),
                     shape.indices.data(), GL_STATIC_DRAW);

        // Vertex attrib: location 0 = position (vec3), stride 5 floats (skip uv)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Attach instance attributes to this VAO
        SetupInstanceAttributes(geo.vao);

        glBindVertexArray(0);

        presetGeos_[preset] = geo;
    }

    PickerBuffer::PresetGeo& PickerBuffer::GetPresetGeo(int preset)
    {
        auto it = presetGeos_.find(preset);
        if (it == presetGeos_.end())
        {
            BuildPresetGeo(preset);
            it = presetGeos_.find(preset);
        }
        return it->second;
    }

    // ─── Instance VBO management ────────────────────────────────────────────

    void PickerBuffer::EnsureInstanceVBO(size_t count)
    {
        size_t needed = count * sizeof(PickerInstanceData);
        if (needed <= instanceVBOCapacity_) return;

        size_t newCap = std::max(needed, instanceVBOCapacity_ * 3 / 2);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(newCap), nullptr, GL_DYNAMIC_DRAW);
        instanceVBOCapacity_ = newCap;
    }

    void PickerBuffer::SetupInstanceAttributes(GLuint vao)
    {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);

        const GLsizei stride = sizeof(PickerInstanceData);

        // mat4 model -> locations 2, 3, 4, 5 (one vec4 per column)
        for (int col = 0; col < 4; ++col)
        {
            GLuint loc = 2 + col;
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, stride,
                                  (void*)(offsetof(PickerInstanceData, model) + col * sizeof(glm::vec4)));
            glVertexAttribDivisor(loc, 1);
        }

        // vec3 idColor -> location 6
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, stride,
                              (void*)offsetof(PickerInstanceData, idColor));
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }

    // -------------------------------------------------------------------------

    // Golden-ratio hue spread: each successive ID gets a hue ~137.5 deg away,
    // producing maximally distinct, fully-saturated colours for small sets.
    glm::vec3 PickerBuffer::IdToColor(uint32_t id)
    {
        constexpr float goldenRatio = 0.618033988749895f;
        const float hue = glm::fract(static_cast<float>(id) * goldenRatio);

        // HSV -> RGB  (S=1, V=1 — always full brightness)
        const float h6 = hue * 6.0f;
        const int   hi = static_cast<int>(h6) % 6;
        const float f  = h6 - static_cast<float>(static_cast<int>(h6));
        const float q  = 1.0f - f;
        const float t  = f;

        switch (hi) {
            case 0: return {1.0f, t,    0.0f};
            case 1: return {q,    1.0f, 0.0f};
            case 2: return {0.0f, 1.0f, t   };
            case 3: return {0.0f, q,    1.0f};
            case 4: return {t,    0.0f, 1.0f};
            default: return {1.0f, 0.0f, q  };
        }
    }

    uint32_t PickerBuffer::PackColor(uint8_t r, uint8_t g, uint8_t b)
    {
        return static_cast<uint32_t>(r)
             | (static_cast<uint32_t>(g) << 8)
             | (static_cast<uint32_t>(b) << 16);
    }

    GLuint PickerBuffer::CompileShader(const std::string& src, GLenum type) const
    {
        GLuint shader = glCreateShader(type);
        const char* c = src.c_str();
        glShaderSource(shader, 1, &c, nullptr);
        glCompileShader(shader);

        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLchar log[512];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            spdlog::error("[PickerBuffer] Shader compile error: {}", log);
        }
        return shader;
    }

    std::string PickerBuffer::LoadFile(const std::string& path) const
    {
        std::ifstream f(path);
        if (!f.is_open()) {
            spdlog::error("[PickerBuffer] Cannot open: {}", path);
            return {};
        }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

} // namespace ettycc
