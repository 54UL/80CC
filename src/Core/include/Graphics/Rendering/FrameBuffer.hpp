#ifndef FRAME_BUFFER_HPP
#define FRAME_BUFFER_HPP

// rendering.cpp
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>

#include <cereal/archives/json.hpp>

namespace ettycc
{
    class FrameBuffer
    {
    private:
        GLuint id_, textureId_, depthBufferId_;
        
    public:
        glm::ivec2 size_;
        glm::ivec2 position_;
        bool initialized_;
        
    public:
        FrameBuffer();
        FrameBuffer(glm::ivec2 position, glm::ivec2 size, bool isDefault);
        ~FrameBuffer();
        
        // FRAME BUFFER API
        GLuint GetId() const;
        GLuint GetTextureId() const;

        void SetSize(glm::ivec2 size);
        glm::ivec2 GetSize(glm::ivec2 size);

        void BeginFrame();
        void EndFrame();

        void Init();
        void CleanUp();

    public:
        // Serialization/Deserialziation
        template <class Archive>
        void serialize(Archive &ar)
        {
            // TODO: IMPLEMENT TO MAKE THIS PROCEDURALLY
            ar(
                cereal::make_nvp("size_x", size_.x),
                cereal::make_nvp("size_y", size_.y),
                cereal::make_nvp("position_x", position_.x),
                cereal::make_nvp("position_y", position_.y));
        }
    };
}

#endif