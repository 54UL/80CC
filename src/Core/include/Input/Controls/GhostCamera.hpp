#ifndef GHOST_CAMERA_HPP
#define GHOST_CAMERA_HPP

#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Input/PlayerInput.hpp>
#include <glm/glm.hpp>
#include "Control.hpp"
#include <spdlog/spdlog.h>

namespace ettycc
{
    class GhostCamera : public Control 
    {
    private:
        PlayerInput *inputSystem_;
        std::shared_ptr<Camera> camera_;

        // ASSUMING MOUSE IMPL...
        glm::vec3 lookAxis;
        glm::vec3 positionAxis;

        glm::ivec2 mouseDelta;
        glm::ivec2 mouseCurrent;

        glm::ivec2 lastAxisPosition;
        float lookSensitivity = 30.0f;
        float movementSensitivity = 50.00f;

	    bool LookCursor=true;
        
    public:
        GhostCamera(PlayerInput *input, std::shared_ptr<Camera> camera);
        ~GhostCamera();

        // Control api
        void Update(float deltaTime) override;
        void LateUpdate(float deltaTime) override;
    };
}
#endif