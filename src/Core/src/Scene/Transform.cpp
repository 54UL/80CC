#include <Scene/Transform.hpp>



namespace ettycc
{
    Transform::Transform()
    {
        this->modelMatrix = glm::mat4(1.0f);
        this->transformMatrix = glm::mat4(1.0f);
        rotationMatrix = glm::mat4(1.0f);
    }

    Transform::~Transform()
    {
        
    }

    void Transform::setGlobalPosition(glm::vec3 position)
    {
        this->modelMatrix = glm::mat4(1);
        this->modelMatrix = glm::translate(this->modelMatrix, position);
        this->transformMatrix =  modelMatrix;
    }

    void Transform::setGlobalRotation(glm::vec3 Euler)
    {
        rotationMatrix = glm::mat4_cast(glm::quat(glm::radians(Euler)));
        transformMatrix = rotationMatrix * modelMatrix ;
    }

    void Transform::translate(glm::vec3 RelativeDirection)
    {
        glm::quat tmpRot(this->modelMatrix);
        modelMatrix = glm::translate(this->modelMatrix, tmpRot * RelativeDirection);
        transformMatrix = modelMatrix;
    }

    void Transform::SetMatrix(glm::mat4 matrix)
    {
        modelMatrix = matrix;
    }

    glm::mat4 Transform::GetMatrix()
    {
        return this->transformMatrix;
    }

    glm::vec3 Transform::getGlobalPosition()
    {
        return modelMatrix[3];
    }

    glm::vec3 Transform::getEulerGlobalRotaion()
    {
        glm::quat tmpRot(modelMatrix);
        // return glm::degrees(glm::eulerAngles(tmpRot));

        return glm::vec3(glm::round(glm::degrees(glm::pitch(tmpRot))),
                        glm::round(glm::degrees(glm::yaw(tmpRot))),
                        glm::round(glm::degrees(glm::roll(tmpRot))));
    }

    glm::quat Transform::getGlobalRotation()
    {
        return glm::quat(modelMatrix);
    }

} // namespace ettycc

