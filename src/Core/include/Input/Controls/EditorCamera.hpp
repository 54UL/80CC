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
    class EditorCamera : public Control 
    {
    private:
        PlayerInput *inputSystem_;
        std::shared_ptr<Camera> camera_;

    public:
        EditorCamera(PlayerInput *input, std::shared_ptr<Camera> camera);
        ~EditorCamera();

        // Control api
        void Update(float deltaTime) override;
        void LateUpdate(float deltaTime) override;
    };
}
#endif