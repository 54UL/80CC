#include <Scene/Transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>


namespace ettycc
{
    Transform::Transform()
    {
        this->modelMatrix = glm::mat4(1.0f);
        this->transformMatrix = glm::mat4(1.0f);
        rotationMatrix =glm::mat4(1.0f);
    }

    Transform::~Transform()
    {
        
    }

    void Transform::setGlobalPosition(glm::vec3 position)
    {
        this->transformMatrix = glm::translate(this->modelMatrix, position);
        this->modelMatrix = transformMatrix;
    }

    void Transform::setGlobalRotation(glm::vec3 Euler)
    {
        // Create a quaternion from Euler angles
        rotationMatrix = glm::mat4_cast(glm::quat(glm::radians(Euler)));
        // Create a rotation matrix from the quaternion
        transformMatrix = rotationMatrix * modelMatrix ;
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

