#include <Input/Controls/OrbitCamera.hpp>
  
namespace ettycc{

    OrbitCamera::OrbitCamera(PlayerInput *input, std::shared_ptr<Camera> camera) :inputSystem_(input), camera_(camera)
    {
       cameraPos = glm::vec3(0.0f, 0.0f, radius);
       cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
       cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
       radius = 5.0f;
       theta = 0.0f;
       phi = 0.0f;
    }

    OrbitCamera::~OrbitCamera()
    {

    }

    void OrbitCamera::Update(float deltaTime)
    {
        lastAxisPosition = inputSystem_->GetLeftAxis();

        // Update camera angles
        theta += sensitivity * static_cast<float>(mouseDelta.x);
        phi += sensitivity * static_cast<float>(-mouseDelta.y);

        // Clamp phi to avoid flipping
        phi = glm::clamp(phi, -89.0f, 89.0f);

        // Update camera position
        glm::vec3 newPos;
        newPos.x = radius * glm::sin(glm::radians(theta)) * glm::cos(glm::radians(phi));
        newPos.y = radius * glm::sin(glm::radians(phi));
        newPos.z = radius * glm::cos(glm::radians(theta)) * glm::cos(glm::radians(phi));
        cameraPos = newPos;

        // Update view matrix
        camera_->underylingTransform.setMatrix(glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp));

        mouseDelta = lastAxisPosition - inputSystem_->GetLeftAxis();
    }

    void OrbitCamera::LateUpdate(float deltaTime)
    {
        
    }
}