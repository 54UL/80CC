#include <Input/Controls/GhostCamera.hpp>
#include <glm/glm.hpp>

namespace ettycc
{
    GhostCamera::GhostCamera(PlayerInput *input, std::shared_ptr<Camera> camera) :inputSystem_(input), camera_(camera)
    {
    }

    GhostCamera::~GhostCamera()
    {

    }

    void GhostCamera::Update(float deltaTime)
    {
        lastAxisPosition = inputSystem_->GetLeftAxis();

        // glm::vec2 translationAxis = inputSystem_->GetLeftAxis();
        // glm::vec3 translation = glm::vec3(translationAxis.x, 0, translationAxis.y) * 1.00f ;
        lookAxis += glm::vec3(mouseDelta.y, mouseDelta.x, 0) * lookSensitivity * deltaTime;

        // Update camera's transform
        // camera_->underylingTransform.translate(translation);
        camera_->underylingTransform.setGlobalRotation(lookAxis);

        mouseDelta = lastAxisPosition - inputSystem_->GetLeftAxis();
    }

    void GhostCamera::LateUpdate(float deltaTime)
    {
        
    }

} // namespace ettycc
