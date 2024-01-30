#ifndef GHOST_CAMERA_HPP
#define GHOST_CAMERA_HPP
#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>

namespace ettycc
{
    class GhostCamera
    {
    private:
        const PlayerInput *inputSystem_;
        const Transform *camera_;

    public:
        GhostCamera(const PlayerInput *input, std::shared_ptr<GhostCamera> camera);
        ~GhostCamera();
        Update(float deltaTime);
    };
}
#endif