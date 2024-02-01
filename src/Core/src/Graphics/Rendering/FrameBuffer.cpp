#include <Graphics/Rendering/FrameBuffer.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    FrameBuffer::FrameBuffer(glm::ivec2 position, glm::ivec2 size, bool isDefault) : position_(position), size_(size), isDefault_(isDefault)
    {
        id_ = 0;
        textureId_ =0;
    }

    FrameBuffer::~FrameBuffer()
    {
        glDeleteFramebuffers(1, &id_);
        glDeleteTextures(1, &textureId_);
        glDeleteRenderbuffers(1, &depthBuffer_);
    }

    void FrameBuffer::BeginFrame()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, id_);
        // Set the viewport to match the framebuffer size
        glViewport(position_.x, position_.y, size_.x, size_.y);

        // Clear the framebuffer with a clear color
        glClearColor(0.4f, 0.2f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // Render your scene here
    }

    void FrameBuffer::EndFrame()
    {
        // Unbind the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void FrameBuffer::Init()
    {
        // Create a custom framebuffer
        glGenFramebuffers(1, &id_);
        glBindFramebuffer(GL_FRAMEBUFFER, id_);

        // Create a texture to render into
        glGenTextures(1, &textureId_);
        glBindTexture(GL_TEXTURE_2D, textureId_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId_, 0);

        // Create and attach a depth buffer
        glGenRenderbuffers(1, &depthBuffer_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size_.x,  size_.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer_);

        // Check if the framebuffer is complete
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            spdlog::error("Frame buffer is not complete!! id: %i",id_);
        }

        // Unbind the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    
    GLuint FrameBuffer::GetId() const
    {
        return id_;
    }
    
    GLuint FrameBuffer::GetTextureId() const
    {
        return textureId_;
    }
    
} // namespace ettycc