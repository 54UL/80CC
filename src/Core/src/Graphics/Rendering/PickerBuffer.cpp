#include <Graphics/Rendering/PickerBuffer.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    PickerBuffer::PickerBuffer(const FrameBuffer& buffer) : size_(buffer.size_), position_(buffer.position_),
                                                            initialized_(false), fb_(buffer), pickingFBO(0),
                                                            pickingTextureId(0),
                                                            pbos{} {
        spdlog::warn("Picker buffer constructed [size: {}, {}] [position: {}, {}] ", size_.x, size_.y, position_.x,
                     position_.y);
    }

    PickerBuffer::~PickerBuffer()
    {
        CleanUp();
    }

    void PickerBuffer::CleanUp()
    {
        glDeleteFramebuffers(1, &pickingTextureId);
        glDeleteTextures(1, &pickingTextureId);
        initialized_ = false;
    }

    void PickerBuffer::BeginFrame()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pickingFBO);
        glViewport(0, 0, fb_.size_.x, fb_.size_.y);

        // glClearBufferuiv(GL_COLOR, 0, &clearValue);

        // glUseProgram(pickingTextureId);

        // this begin frame should start before rendering objects with other shaders...
        // for (auto& obj : objects) {
        //     glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, &obj.mvp[0][0]);
        //     glUniform1ui(idLoc, obj.id);
        //
        //     glBindVertexArray(obj.vao);
        //     glDrawElements(GL_TRIANGLES, obj.indexCount,
        //                    GL_UNSIGNED_INT, 0);
        // }
    }

    void PickerBuffer::EndFrame()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool PickerBuffer::CheckHovered(glm::vec2& position) {
        int nextIndex = (pboIndex + 1) % 2;
        bool isHovered = false;

        // Request pixel into current PBO
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[pboIndex]);
        glReadPixels(position.x, position.y, 1, 1,
                     GL_RED_INTEGER, GL_UNSIGNED_INT, 0);

        // Map previous PBO (async, from last frame)
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[nextIndex]);
        uint32_t* ptr = (uint32_t*)glMapBuffer(GL_PIXEL_PACK_BUFFER,
                                               GL_READ_ONLY);
        if (ptr) {
            uint32_t pickedID = *ptr;
            if (pickedID != 0) {
                isHovered = true;
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        pboIndex = nextIndex;
        return isHovered;
    }

    void PickerBuffer::Init()
    {
        if (initialized_)
        {
            spdlog::warn("Frame buffer already initialized id: [{}]", pickingFBO);
            return;
        }

        glGenFramebuffers(1, &pickingFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, pickingFBO); // This uses a frame buffer but it's way to diferent from our class implementation.

        glGenTextures(1, &pickingTextureId);
        glBindTexture(GL_TEXTURE_2D, pickingTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, fb_.size_.x, fb_.size_.y, 0,
                     GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, pickingTextureId, 0);

        glGenBuffers(2, pbos);
        for (const unsigned int pbo : pbos) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(uint32_t),
                         nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    GLuint PickerBuffer::GetId() const
    {
        return pickingFBO;
    }

    GLuint PickerBuffer::GetTextureId() const
    {
        return pickingTextureId;
    }

    void PickerBuffer::SetSize(glm::ivec2 size)
    {
        if (size_ != size)
        {
            size_ = size;
            CleanUp();
            Init();
        }
    }

    glm::ivec2 PickerBuffer::GetSize() const
    {
        return fb_.size_;
    }

} // namespace ettycc