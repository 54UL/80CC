#include <Graphics/Rendering/FrameBuffer.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    FrameBuffer::FrameBuffer()
    {
        id_ = 0;
        textureId_ = 0;
        depthBufferId_ = 0;
        initialized_ = 0;
        spdlog::warn("Empty frame buffer constructed");
    }

    FrameBuffer::FrameBuffer(glm::ivec2 position, glm::ivec2 size, bool isDefault) : position_(position), size_(size)
    {
        id_ = 0;
        textureId_ = 0;
        depthBufferId_ = 0;
        initialized_ = 0;
        spdlog::warn("Seeded frame buffer constructed [size: {}, {}] [position: {}, {}] [isDefault: {}] ", size.x, size.y, position.x, position.y, isDefault);
    }

    FrameBuffer::~FrameBuffer()
    {
        CleanUp();
    }

    void FrameBuffer::CleanUp()
    {
        glDeleteFramebuffers(1, &id_);
        glDeleteTextures(1, &textureId_);
        glDeleteRenderbuffers(1, &depthBufferId_);
        initialized_ = false;
    }

    void FrameBuffer::BeginFrame()
    {
        constexpr const float lightGray = 0X1E / 100.0f;

        glClearColor(lightGray, lightGray,lightGray, 1.0f);
        glBindFramebuffer(GL_FRAMEBUFFER, id_);
        glViewport(position_.x, position_.y, size_.x, size_.y);

        GLenum glError = glGetError();
        if (glError != GL_NO_ERROR)
        {
            // this is catching more than viewport binding errors, thankfully...
            spdlog::error("Begin frame buffer bind OpenGL error: {}", glError);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
    }

    void FrameBuffer::EndFrame()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBuffer::Init()
    {
        if (initialized_)
        {
            spdlog::warn("Frame buffer already initialized id: [{}]", id_);
            return;
        }
        
        glGenFramebuffers(1, &id_);
        glBindFramebuffer(GL_FRAMEBUFFER, id_);

        // Create a texture to render into
        glGenTextures(1, &textureId_);
        glBindTexture(GL_TEXTURE_2D, textureId_);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId_, 0);

        GLenum textureError = glGetError();
        if (textureError != GL_NO_ERROR)
        {
            // TODO: OPEN GL 1281 ERROR WHEN RUNNING THIS BEFORE RUNNING THE MAIN GRAPHICS LOOP
            spdlog::error("FrameBuffer, textureError {}", textureError);
        }
           
        // Create and attach a depth buffer
        glGenRenderbuffers(1, &depthBufferId_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBufferId_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size_.x, size_.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBufferId_);

        // Check for renderbuffer creation errors
        GLenum renderbufferError = glGetError();
        if (renderbufferError != GL_NO_ERROR)
        {
            spdlog::error("Render buffer OpenGL error: {}", renderbufferError);
        }

        // Check if the framebuffer is complete
        GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
        {
            spdlog::error("Frame buffer is not complete!! id: {}", id_);
        }
        else 
        {
            initialized_ = true;
        }

        // Unbind the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    GLuint FrameBuffer::GetId() const
    {
        return id_;
    }
    
    GLuint FrameBuffer::GetTextureId() const
    {
        return textureId_;
    }

    void FrameBuffer::SetSize(glm::ivec2 size)
    {
        if (size_ != size)
        {
            size_ = size;
            CleanUp();
            Init();
        }
    }

    glm::ivec2 FrameBuffer::GetSize(glm::ivec2 size)
    {
        return size_;
    }

} // namespace ettycc