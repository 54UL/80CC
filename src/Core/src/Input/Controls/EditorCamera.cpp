#include <Input/Controls/EditorCamera.hpp>
#include <glm/glm.hpp>
#include <iostream>

namespace ettycc
{
    EditorCamera::EditorCamera(PlayerInput *input, std::shared_ptr<Camera> camera) :inputSystem_(input), camera_(camera)
    {
        
    }

    EditorCamera::~EditorCamera()
    {

    }
   
    void EditorCamera::Update(float deltaTime)
    {
        // camera_->underylingTransform.setGlobalPosition(positionAxis);
        // camera_->underylingTransform.setGlobalRotation(lookAxis);
    }

    void EditorCamera::LateUpdate(float deltaTime)
    {

    }
} // namespace ettycc
