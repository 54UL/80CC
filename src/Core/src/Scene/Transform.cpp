#include <Scene/Transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>


namespace ettycc
{
    Transform::Transform()
    {
        this->modelMatrix = glm::mat4(1.0f);
    }

    Transform::~Transform()
    {
        
    }

    void Transform::setGlobalPosition(glm::vec3 position)
    {
        this->modelMatrix = glm::translate(this->modelMatrix, position);
    }

    void Transform::setLocalPosition(glm::vec3 position)
    {
        this->modelMatrix = glm::translate(this->modelMatrix, position);
    }

    void Transform::setGlobalRotation(glm::vec3 Euler)
    {
        auto RotationMatrix = glm::mat4_cast(glm::quat(glm::vec3(glm::radians(Euler.x), glm::radians(Euler.y), glm::radians(Euler.z))));
        modelMatrix = modelMatrix * RotationMatrix;
    }

    void Transform::setGlobalRotation(glm::vec3 Axis, float Angle)
    {
        auto RotationMatrix = glm::mat4(1);
        RotationMatrix = glm::rotate(RotationMatrix, Angle, Axis);
        modelMatrix = modelMatrix * RotationMatrix;
    }

    void Transform::setLocalRotation(glm::quat rotQuat)
    {
        auto RotationMatrix = glm::mat4_cast(rotQuat);
        this->modelMatrix = modelMatrix * RotationMatrix;
    }

    void Transform::translate(glm::vec3 RelativeDirection)
    {
        glm::quat tmpRot(this->modelMatrix);
        modelMatrix = glm::translate(this->modelMatrix, tmpRot * RelativeDirection);
    }

    void Transform::SetMatrix(glm::mat4 matrix)
    {
        modelMatrix = matrix;
    }

    glm::mat4 Transform::GetMatrix()
    {
        return this->modelMatrix;
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

