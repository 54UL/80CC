#ifndef ORBIT_CAMERA_HPP
#define ORBIT_CAMERA_HPP
#include <Scene/Transform.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Input/PlayerInput.hpp>

namespace ettycc
{
  class OrbitCamera : public Control 
  {
  private:
    PlayerInput *inputSystem_;
    std::shared_ptr<Camera> camera_;
    
    glm::vec3 cameraPos;
    glm::vec3 cameraFront;
    glm::vec3 cameraUp;
    const float sensitivity = 0.05f;

    float radius;
    float theta;
    float phi;
    
  public:
    OrbitCamera(PlayerInput *input, std::shared_ptr<Camera> camera);
    ~OrbitCamera();

    // Control api
    void Update(float deltaTime) override;
    void LateUpdate(float deltaTime) override;
  };  
}


#endif
