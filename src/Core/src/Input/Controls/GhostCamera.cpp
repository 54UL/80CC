#include <Input/Controls/GhostCamera.hpp>
#include <glm/glm.hpp>
#include <iostream>

namespace ettycc
{
    static float time;
    GhostCamera::GhostCamera(PlayerInput *input, std::shared_ptr<Camera> camera) :inputSystem_(input), camera_(camera)
    {
        time = 0;
        lastAxisPosition = glm::ivec2(1,1);
        lookAxis = glm::vec3(365.0f,1.0f,1.0f);
        positionAxis = glm::vec3(0.0f,0.0f,-2.0f);
    }

    GhostCamera::~GhostCamera()
    {

    }
   
    void GhostCamera::Update(float deltaTime)
    {
        mouseCurrent = inputSystem_->GetMousePos();
        mouseDelta =  mouseCurrent - lastAxisPosition;
        lastAxisPosition = mouseCurrent;

        if (inputSystem_->GetMouseButton(3)) {
            positionAxis += glm::vec3(inputSystem_->GetLeftAxis().x , inputSystem_->GetLeftAxis().y, 0.0f) * movementSensitivity * deltaTime;
            lookAxis += glm::vec3(-mouseDelta.y, mouseDelta.x, 0) * lookSensitivity * deltaTime;
        }

        camera_->underylingTransform.setGlobalPosition(positionAxis);
        camera_->underylingTransform.setGlobalRotation(lookAxis);
    }

    void GhostCamera::LateUpdate(float deltaTime)
    {

    }
} // namespace ettycc
