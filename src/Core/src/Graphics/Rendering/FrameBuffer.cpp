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
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        glBindFramebuffer(GL_FRAMEBUFFER, id_);
        glViewport(position_.x, position_.y, size_.x, size_.y);

        GLenum glError = glGetError();
        if (glError != GL_NO_ERROR)
        {
            spdlog::error("Begin frame buffer bind OpenGL error: {}", glError);
        }
        // begin gl frame config
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // we're not using the stencil buffer now
        glEnable(GL_DEPTH_TEST);

            // Check for OpenGL errors
   
    }

    void FrameBuffer::EndFrame()
    {

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBuffer::Init()
    {
        // Create a custom framebuffer
        glGenFramebuffers(1, &id_);
        glBindFramebuffer(GL_FRAMEBUFFER, id_);

        // Before texture creation
        GLenum globalErrorBefore = glGetError();
        if (globalErrorBefore != GL_NO_ERROR)
        {
            spdlog::error("Global OpenGL error before texture creation: {}", globalErrorBefore);
        }
            // Create a texture to render into
        glGenTextures(1, &textureId_);
        glBindTexture(GL_TEXTURE_2D, textureId_);

         // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
         // Check for texture creation errors
        GLenum textureError = glGetError();
        if (textureError != GL_NO_ERROR)
        {
            spdlog::error("Create texture OpenGL error: {}", textureError);
        }
       

        // Unbind the texture
        glBindTexture(GL_TEXTURE_2D, 0);
     

        // Create and attach a depth buffer
        glGenRenderbuffers(1, &depthBuffer_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size_.x, size_.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer_);

        // Check for renderbuffer creation errors
        GLenum renderbufferError = glGetError();
        if (renderbufferError != GL_NO_ERROR)
        {
            spdlog::error("Render buffer OpenGL error: {}", renderbufferError);
        }

        // Check if the framebuffer is complete
        GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (framebufferStatus == GL_FRAMEBUFFER_COMPLETE)
        {
            spdlog::warn("Frame buffer is complete!! id: {}", id_);
        }
        else
        {
            spdlog::error("Frame buffer is not complete!! id: {}", id_);
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