#ifndef GHOST_CAMERA_HPP
#define GHOST_CAMERA_HPP

#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Input/PlayerInput.hpp>
#include <glm/glm.hpp>
#include "Control.hpp"

namespace ettycc
{
    class GhostCamera : public Control 
    {
    private:
        PlayerInput *inputSystem_;
        std::shared_ptr<Camera> camera_;

        // ASSUMING MOUSE IMPL...
        glm::vec3 lookAxis;
        glm::vec2 mouseDelta;
        glm::vec2 lastAxisPosition;
        float lookSensitivity = 1.1f;
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