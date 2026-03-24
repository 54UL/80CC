#include <Input/Controls/EditorCamera.hpp>
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

    // -------------------------------------------------------------------------

    glm::mat4 EditorCamera::ComputeViewMatrix(float /*deltaTime*/) const
    {
        // Pure translation — zoom lives only in the projection
        return glm::translate(glm::mat4(1.0f), glm::vec3(-position, 0.0f));
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
        if (!enabled) return;


        if (inputSystem_->GetMouseButton(static_cast<int>(MouseButton::LEFT)))
        {
            const glm::vec2 pixelDelta = inputSystem_->GetMouseDelta();
            if (pixelDelta.x != 0.0f || pixelDelta.y != 0.0f)
            {
                const float h = static_cast<float>(frame_buffer_->size_.y);
                if (h > 0.0f)
                {
                    // 1 pixel = (2 * halfH) / screenHeight world units
                    const float worldPerPixel = (2.0f * baseSize_ / zoom) / h;
                    position.x -= pixelDelta.x * worldPerPixel;
                    position.y += pixelDelta.y * worldPerPixel;
                }
            }
        }

        // --- ZOOM (scroll wheel) -----------------------------------------------
        // GetWheelY() is reset every frame, so it's the delta for this frame only.
        const int wheel = inputSystem_->GetWheelY();
        if (wheel != 0)
        {
            // Fixed multiplicative step per tick — no deltaTime, no overshoot
            constexpr float zoomStep = 1.12f;
            const float factor = (wheel > 0) ? zoomStep : (1.0f / zoomStep);

            // Zoom toward the cursor: keep the world point under the cursor fixed.
            const float w = static_cast<float>(frame_buffer_->size_.x);
            const float h = static_cast<float>(frame_buffer_->size_.y);
            if (w > 0.0f && h > 0.0f)
            {
                const glm::vec2 mousePos = glm::vec2(inputSystem_->GetMousePos());
                // Cursor in NDC [-1, 1] relative to viewport centre
                const glm::vec2 cursorNDC = (mousePos - glm::vec2(w, h) * 0.5f)
                                          / (glm::vec2(w, h) * 0.5f);

                const float aspect  = w / h;
                const float halfH   = baseSize_ / zoom;
                const float halfW   = halfH * aspect;
                // World position currently under the cursor
                const glm::vec2 worldCursor = position
                    + glm::vec2(cursorNDC.x * halfW, -cursorNDC.y * halfH);

                zoom *= factor;
                zoom  = glm::clamp(zoom, 0.05f, 500.0f);

                // Recompute half-extents after zoom and shift position so the
                // same world point stays under the cursor
                const float newHalfH = baseSize_ / zoom;
                const float newHalfW = newHalfH * aspect;
                position = worldCursor
                    - glm::vec2(cursorNDC.x * newHalfW, -cursorNDC.y * newHalfH);
            }
            else
            {
                zoom *= factor;
                zoom  = glm::clamp(zoom, 0.05f, 500.0f);
            }
        }
    }

    void EditorCamera::LateUpdate(float /*deltaTime*/) {}

    // handlePan / handleZoom kept but no longer called from Update
    void EditorCamera::handleZoom(float /*wheelDelta*/, float /*dt*/) {}
    void EditorCamera::handlePan(glm::vec2 /*left*/, glm::vec2 /*right*/, float /*dt*/) {}

} // namespace ettycc
