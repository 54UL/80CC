#ifndef EDITOR_CAMERA_HPP
#define EDITOR_CAMERA_HPP

#include <Scene/Transform.hpp>
#include <Input/PlayerInput.hpp>
#include <glm/glm.hpp>
#include "Control.hpp"

#include <GRaphics/Rendering/FrameBuffer.hpp>

namespace ettycc
{
    class EditorCamera : public Control
    {
    private:
        PlayerInput *inputSystem_;
        FrameBuffer *frame_buffer_;
        
    public:
        EditorCamera(PlayerInput *input, FrameBuffer* frameBuffer);
        ~EditorCamera();

        // just for testing....
        glm::vec2 position = {0.0f, 0.0f};
        float zoom = 1.0f;
        bool enabled = false;

        [[nodiscard]] glm::mat4 ComputeViewMatrix(float deltaTime) const;
        [[nodiscard]] glm::mat4 ComputeProjectionMatrix(float deltaTime) const;

        // Control api
        void Update(float deltaTime) override;
        void LateUpdate(float deltaTime) override;

    private:
        void EditorCamera::handleZoom(float wheelDelta, float dt);
        void EditorCamera::handlePan(glm::vec2 leftAxis, glm::vec2 rightAxis, float dt);
    };
}
#endif