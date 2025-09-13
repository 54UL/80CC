#include <Scene/Transform.hpp>



namespace ettycc
{
    Transform::Transform() {
        this->modelMatrix = glm::mat4(1.0f);
        this->transformMatrix = glm::mat4(1.0f);
        // TODO: IF CONSTRUCTED ONLY BY VECTORS CONVERT VECTOR DATA INTO MATRIX DATA, AND WHEN ACCESING THE DATA AS VECTORS CONVERT MATRIX DATA -> VECTOR DATA
        rotationMatrix = glm::mat4(1.0f);
        rotation_ = glm::vec3(0.0f, 0.0f, 0.0f);
        position_ = glm::vec3(0.0f, 0.0f, 0.0f);
        scale_ = glm::vec3(1.0f, 1.0f, 1.0f);
        enabled = true;
    }

    Transform::~Transform()
    {
        
    }

    void Transform::setGlobalPosition(const glm::vec3 position)
    {
        position_ = position; 
        this->modelMatrix = glm::mat4(1);
        this->modelMatrix = glm::translate(this->modelMatrix, position);
        this->transformMatrix =  modelMatrix;
    }

    void Transform::setGlobalRotation(const glm::vec3 Euler)
    {
        rotation_ = Euler;
        rotationMatrix = glm::mat4_cast(glm::quat(glm::radians(Euler)));
        transformMatrix = rotationMatrix * modelMatrix ;
    }

    void Transform::setGlobalScale(const glm::vec3 scale)
    {
        scale_ = scale;
        this->modelMatrix = glm::scale(this->modelMatrix, scale);
        this->transformMatrix = modelMatrix;
    }

    void Transform::translate(const glm::vec3 RelativeDirection)
    {
        glm::quat tmpRot(this->modelMatrix);
        modelMatrix = glm::translate(this->modelMatrix, tmpRot * RelativeDirection);
        transformMatrix = modelMatrix;
    }

    void Transform::SetMatrix(const glm::mat4 &matrix)
    {
        modelMatrix = matrix;
    }

    glm::mat4 Transform::GetMatrix() const
    {
        return transformMatrix;
    }

    glm::vec3 Transform::getGlobalPosition() const
    {
        return modelMatrix[3];
    }

    glm::quat Transform::getGlobalRotation() const
    {
        return glm::quat(modelMatrix);
    }

    glm::vec3 Transform::getGlobalScale() const
    {
        return scale_;
    }

    glm::vec3 Transform::getEulerGlobalRotation() const
    {
        glm::vec3 eulerAngles = glm::eulerAngles(glm::quat_cast(modelMatrix));

        return glm::degrees(eulerAngles);
    }
} // namespace ettycc

