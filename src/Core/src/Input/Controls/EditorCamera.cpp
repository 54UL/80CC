#include <Input/Controls/EditorCamera.hpp>
#include <Scene/Components/CameraControllerComponent.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>

namespace ettycc
{
    EditorCamera::EditorCamera(PlayerInput* input, FrameBuffer* frame_buffer)
        : inputSystem_(input), frame_buffer_(frame_buffer)
    {
    }

    EditorCamera::~EditorCamera() = default;

    void EditorCamera::BindComponent(CameraControllerComponent* comp)
    {
        linkedComponent_ = comp;
        if (comp)
        {
            zoom = comp->zoom;
        }
    }

    void EditorCamera::BindTransform(Transform* transform)
    {
        linkedTransform_ = transform;
    }

    glm::vec2 EditorCamera::GetPosition() const
    {
        if (linkedTransform_)
        {
            auto p = linkedTransform_->getGlobalPosition();
            return { p.x, p.y };
        }
        return { 0.f, 0.f };
    }

    // -------------------------------------------------------------------------

    glm::mat4 EditorCamera::ComputeViewMatrix(float /*deltaTime*/) const
    {
        glm::vec2 pos = GetPosition();
        return glm::translate(glm::mat4(1.0f), glm::vec3(-pos, 0.0f));
    }

    glm::mat4 EditorCamera::ComputeProjectionMatrix(float /*deltaTime*/) const
    {
        const float w = static_cast<float>(frame_buffer_->size_.x);
        const float h = static_cast<float>(frame_buffer_->size_.y);
        const float aspect = (h > 0.0f) ? w / h : 1.0f;

        const float halfH = baseSize_ / zoom;
        const float halfW = halfH * aspect;

        return glm::ortho(-halfW, halfW, -halfH, halfH, -100.0f, 100.0f);
    }

    // -------------------------------------------------------------------------

    void EditorCamera::Update(float /*deltaTime*/)
    {
        if (!enabled || !linkedTransform_) return;

        glm::vec2 position = GetPosition();

        // Read zoom limits from linked component if available
        float clampMin = 0.05f;
        float clampMax = 500.f;
        float zoomStep = 1.12f;
        if (linkedComponent_)
        {
            clampMin = linkedComponent_->zoomMin;
            clampMax = linkedComponent_->zoomMax;
            zoomStep = linkedComponent_->zoomSpeed;
        }

        if (inputSystem_->GetMouseButton(static_cast<int>(MouseButton::MIDDLE)))
        {
            const glm::vec2 pixelDelta = inputSystem_->GetMouseDelta();
            if (pixelDelta.x != 0.0f || pixelDelta.y != 0.0f)
            {
                const float h = static_cast<float>(frame_buffer_->size_.y);
                if (h > 0.0f)
                {
                    const float worldPerPixel = (2.0f * baseSize_ / zoom) / h;
                    position.x -= pixelDelta.x * worldPerPixel;
                    position.y += pixelDelta.y * worldPerPixel;
                }
            }
        }

        // --- ZOOM (scroll wheel) -----------------------------------------------
        const int wheel = inputSystem_->GetWheelY();
        if (wheel != 0)
        {
            const float factor = (wheel > 0) ? zoomStep : (1.0f / zoomStep);

            const float w = static_cast<float>(frame_buffer_->size_.x);
            const float h = static_cast<float>(frame_buffer_->size_.y);
            if (w > 0.0f && h > 0.0f)
            {
                const glm::vec2 mousePos = glm::vec2(inputSystem_->GetMousePos());
                const glm::vec2 cursorNDC = (mousePos - glm::vec2(w, h) * 0.5f)
                                          / (glm::vec2(w, h) * 0.5f);

                const float aspect  = w / h;
                const float halfH   = baseSize_ / zoom;
                const float halfW   = halfH * aspect;
                const glm::vec2 worldCursor = position
                    + glm::vec2(cursorNDC.x * halfW, -cursorNDC.y * halfH);

                zoom *= factor;
                zoom  = glm::clamp(zoom, clampMin, clampMax);

                const float newHalfH = baseSize_ / zoom;
                const float newHalfW = newHalfH * aspect;
                position = worldCursor
                    - glm::vec2(cursorNDC.x * newHalfW, -cursorNDC.y * newHalfH);
            }
            else
            {
                zoom *= factor;
                zoom  = glm::clamp(zoom, clampMin, clampMax);
            }
        }

        // Write position back to the linked transform (single source of truth)
        linkedTransform_->setGlobalPosition(glm::vec3(position, 0.f));

        // Write zoom back to linked component so it's inspectable/serializable
        if (linkedComponent_)
        {
            linkedComponent_->zoom = zoom;
        }
    }

    void EditorCamera::LateUpdate(float /*deltaTime*/) {}

    void EditorCamera::handleZoom(float /*wheelDelta*/, float /*dt*/) {}
    void EditorCamera::handlePan(glm::vec2 /*left*/, glm::vec2 /*right*/, float /*dt*/) {}

} // namespace ettycc
