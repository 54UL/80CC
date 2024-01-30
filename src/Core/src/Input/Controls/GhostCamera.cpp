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
    // Get input for translation
        glm::vec2 translationAxis = inputSystem_->GetLeftAxis();
        glm::vec3 translation = glm::vec3(translationAxis.x, 0, translationAxis.y) * 1.00f ;

        // Get input for rotation
        glm::vec2 rotationAxis = inputSystem_->GetRightAxis();
        // glm::vec3 currentRotation = camera_->underylingTransform.getEulerGlobalRotaion();
        glm::vec3 rotation = glm::vec3(rotationAxis.y, rotationAxis.x, 0) * 0.1f;

        // Update camera's transform
        // camera_->underylingTransform.translate(translation);
        camera_->underylingTransform.setGlobalRotation(rotation);
    }
} // namespace ettycc
