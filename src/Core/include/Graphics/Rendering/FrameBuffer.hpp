#ifndef FRAME_BUFFER_HPP
#define FRAME_BUFFER_HPP

// rendering.cpp
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>

namespace ettycc
{
    class FrameBuffer
    {
    private:
        GLuint id_,textureId_,depthBuffer_;

    public:
        glm::ivec2 size_;
        glm::ivec2 position_;
        bool isDefault_;
    public:
        FrameBuffer(glm::ivec2 position, glm::ivec2 size, bool isDefault);
        ~FrameBuffer();
        
        // FRAME BUFFER API
        GLuint GetId() const;
        GLuint GetTextureId() const;

        void BeginFrame();
        void EndFrame();

        void Init();
    };
}

#endif