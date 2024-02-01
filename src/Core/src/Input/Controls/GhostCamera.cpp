#include <Input/Controls/GhostCamera.hpp>
#include <glm/glm.hpp>
#include <iostream>
namespace ettycc
{
    static float time;
    GhostCamera::GhostCamera(PlayerInput *input, std::shared_ptr<Camera> camera) :inputSystem_(input), camera_(camera)
    {
        time = 0;
        lastAxisPosition = glm::ivec2(0,0);
    }

    GhostCamera::~GhostCamera()
    {

    }
   
    void GhostCamera::Update(float deltaTime)
    {
        mouseDelta = lastAxisPosition - inputSystem_->GetMousePos();
        lastAxisPosition = inputSystem_->GetMousePos();
        // glm::vec2 translationAxis = inputSystem_->GetLeftAxis();
        // glm::vec3 translation = glm::vec3(translationAxis.x, 0, translationAxis.y) * 1.00f ;
        lookAxis += glm::vec3(mouseDelta.x, mouseDelta.y, 0) ;

        // Update camera's transform
        // camera_->underylingTransform.translate(translation);
        // camera_->underylingTransform.setGlobalRotation(lookAxis);
        
        // camera_->underylingTransform.setGlobalRotation(glm::vec3(0,time+=(deltaTime*0.52f),0));
        // camera_->underylingTransform.setGlobalPosition(glm::vec3(0,time+=(deltaTime*0.52f),0));

        

    }

    void GhostCamera::LateUpdate(float deltaTime)
    {
    }

} // namespace ettycc
