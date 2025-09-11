#include <Input/Controls/EditorCamera.hpp>
#include <glm/glm.hpp>

#include "spdlog/spdlog.h"

namespace ettycc
{
    EditorCamera::EditorCamera(PlayerInput *input, FrameBuffer* frame_buffer)  :inputSystem_(input), frame_buffer_(frame_buffer)
    {
        
    }

    EditorCamera::~EditorCamera() = default;

    void EditorCamera::handlePan(glm::vec2 leftAxis, glm::vec2 rightAxis, float dt) 
    {
        float panSpeed = 500.0f * dt / zoom;

        position += leftAxis * panSpeed;
        position -= rightAxis * panSpeed; // opposite direction
    }

    void EditorCamera::handleZoom(float wheelDelta, float dt) 
    {
        constexpr float zoomSpeed = 10.0f;
        zoom *= (1.0f + wheelDelta * zoomSpeed * dt);

        // clamp
        if (zoom < 0.1f) zoom = 0.1f;
        // if (zoom > 10.0f) zoom = 10.0f;
    }

    glm::mat4 EditorCamera::ComputeViewMatrix(float deltaTime) const
    {
        return glm::translate(glm::mat4(1), glm::vec3((-position) / zoom, 0.0f));
    }

    glm::mat4 EditorCamera::ComputeProjectionMatrix(float deltaTime) const
    {
        const float screenWidth = frame_buffer_->size_.x;
        const float screenHeight = frame_buffer_->size_.y;

        const auto ortho = glm::ortho(-1.0f/zoom, 1.0f / zoom, -1.0f/zoom, 1.0f / zoom, -1.0f, 1.0f);
        return ortho;
    }

    void EditorCamera::Update(float deltaTime)
    {
        // handlePan(inputSystem_->GetLeftAxis(), inputSystem_->GetRightAxis(), deltaTime);
        if (inputSystem_->GetMouseButton(MouseButton::LEFT)) // Left mouse button is pressed
        {
            handlePan(inputSystem_->GetLeftAxis(), inputSystem_->GetRightAxis(), deltaTime);
        }

        static int lastWheelY = 0;
        if (inputSystem_->GetWheelY() != lastWheelY) {
            lastWheelY = inputSystem_->GetWheelY();
            handleZoom( inputSystem_->GetWheelY(), deltaTime);
        }
    }

    void EditorCamera::LateUpdate(float deltaTime)
    {

    }

} // namespace ettycc
