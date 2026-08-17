#ifndef EDITOR_CAMERA_HPP
#define EDITOR_CAMERA_HPP

#include <Scene/Transform.hpp>
#include <Input/PlayerInput.hpp>
#include <glm/glm.hpp>
#include "Control.hpp"

#include <Graphics/Rendering/FrameBuffer.hpp>

namespace ettycc
{
    struct CameraControllerComponent;

    // Generic 2D pan/zoom control.  Operates on any SceneNode's transform
    // (not camera-specific).  When a CameraControllerComponent is linked,
    // zoom limits and speed are read from it, and zoom is written back.
    //
    // The position is ALWAYS the linked transform's XY -- this control
    // never maintains its own parallel position.
    class EditorCamera : public Control
    {
    private:
        PlayerInput *inputSystem_;
        FrameBuffer *frame_buffer_;

    public:
        EditorCamera(PlayerInput *input, FrameBuffer* frameBuffer);
        ~EditorCamera();

        // The transform this control reads/writes position to.
        // Must be set before Update() has any effect.
        Transform* linkedTransform_ = nullptr;

        float zoom    = 1.0f;
        bool  enabled = false;

        // Optional link to a CameraControllerComponent on the same entity.
        // When set, Update() reads zoom limits and writes zoom back so the
        // values are inspectable and serializable.
        CameraControllerComponent* linkedComponent_ = nullptr;
        void BindComponent(CameraControllerComponent* comp);
        void BindTransform(Transform* transform);

        // Control api
        void Update(float deltaTime) override;
        void LateUpdate(float deltaTime) override;
        // world units visible as half-height at zoom = 1
        static constexpr float baseSize_ = 5.0f;

        // Helper: read position from the linked transform (or 0,0)
        glm::vec2 GetPosition() const;

        [[nodiscard]] glm::mat4 ComputeViewMatrix(float deltaTime) const;
        [[nodiscard]] glm::mat4 ComputeProjectionMatrix(float deltaTime) const;

    private:
        void handleZoom(float wheelDelta, float dt);
        void handlePan(glm::vec2 leftAxis, glm::vec2 rightAxis, float dt);
    };
}
#endif
