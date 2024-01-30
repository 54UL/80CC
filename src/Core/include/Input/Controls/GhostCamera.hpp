#ifndef GHOST_CAMERA_HPP
#define GHOST_CAMERA_HPP
#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Input/PlayerInput.hpp>

namespace ettycc
{
    class GhostCamera
    {
    private:
        PlayerInput *inputSystem_;
        std::shared_ptr<Camera> camera_;

    public:
        GhostCamera(PlayerInput *input, std::shared_ptr<Camera> camera);
        ~GhostCamera();
        void Update(float deltaTime);
    };
}
#endif