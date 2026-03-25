#include <Graphics/Rendering/PickerBuffer.hpp>
#include <Graphics/Rendering/Renderable.hpp>
#include <Graphics/Rendering/RenderingContext.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace ettycc
{
    PickerBuffer::PickerBuffer(glm::ivec2 size) : size_(size) {}

    PickerBuffer::~PickerBuffer() { CleanUp(); }

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

        glUseProgram(program_);

        // ID 0 = background. Renderables start at ID 1.
        for (uint32_t i = 0; i < static_cast<uint32_t>(renderables.size()); ++i)
        {
            const uint32_t  id  = i + 1;
            const glm::vec3 col = IdToColor(id);

            // Store quantised RGB → id in lookup table
            const auto r8 = static_cast<uint8_t>(col.r * 255.0f + 0.5f);
            const auto g8 = static_cast<uint8_t>(col.g * 255.0f + 0.5f);
            const auto b8 = static_cast<uint8_t>(col.b * 255.0f + 0.5f);
            colorToId_[PackColor(r8, g8, b8)] = id;

            glUniform3f(idColorLoc_, col.r, col.g, col.b);
            renderables[i]->DrawForPicker(ctx, program_, id);
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

    // -------------------------------------------------------------------------

    // Golden-ratio hue spread: each successive ID gets a hue ~137.5° away,
    // producing maximally distinct, fully-saturated colours for small sets.
    glm::vec3 PickerBuffer::IdToColor(uint32_t id)
    {
        constexpr float goldenRatio = 0.618033988749895f; // MAKE CONSTANT
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
