#include <Input/Controls/GhostCamera.hpp>


namespace ettycc
{
    GhostCamera::GhostCamera(const PlayerInput *input, const Transform *camera)
    {
    }

    GhostCamera::~GhostCamera()
    {
    }

    GhostCamera::Update(float deltaTime)
    {
        auto translationAxis = input->GetLeftAxis();
        camera_->setGlobalRotation(input->GetRightAxis(), 0);
        glm::vec3 position = glm::vec3(translationAxis.x, 0, translationAxis.y);
        camera_->translate(position);
    }
} // namespace ettycc
