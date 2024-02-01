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
        mouseCurrent = inputSystem_->GetMousePos();
        mouseDelta = lastAxisPosition - mouseCurrent;
        lastAxisPosition = mouseCurrent;

        // glm::vec2 translationAxis = inputSystem_->GetLeftAxis();
        // glm::vec3 translation = glm::vec3(translationAxis.x, 0, translationAxis.y) * 1.00f ;
        lookAxis += glm::vec3(mouseDelta.x*0.1f, 0, 0) ;
        // std::cout<<mouseDelta.x<<" y"<<mouseDelta.y<<"\n";
        // camera_->underylingTransform.setGlobalRotation();

        camera_->underylingTransform.setGlobalRotation(glm::vec3(0,time+=(deltaTime*0.52f),0));
        // camera_->underylingTransform.setGlobalPosition(glm::vec3(0,time+=(deltaTime*0.52f),0));
    }

    void GhostCamera::LateUpdate(float deltaTime)
    {
    }

} // namespace ettycc
