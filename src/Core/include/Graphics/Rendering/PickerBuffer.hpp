#ifndef PICK_BUFFER_HPP
#define PICK_BUFFER_HPP

// rendering.cpp

#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>

#include <cereal/archives/json.hpp>

#include <Graphics/Rendering/FrameBuffer.hpp>

namespace ettycc
{
    class PickerBuffer
    {
    private:
        FrameBuffer fb_;
        GLuint pickingFBO, pickingTextureId;
        GLuint pbos[2];
        int pboIndex = 0;

    public:
        glm::ivec2 size_;
        glm::ivec2 position_;
        bool initialized_;

    public:
        PickerBuffer(const FrameBuffer &buffer);

        void CleanUp();
        void Init();
        void BeginFrame();

        void SetSize(glm::ivec2 size);
        glm::ivec2 GetSize() const;

        static void EndFrame();
        bool CheckHovered(glm::vec2& position);

        GLuint GetId() const;
        GLuint GetTextureId() const;

        // PickerBuffer(glm::ivec2 position, glm::ivec2 size, bool isDefault);
        ~PickerBuffer();

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