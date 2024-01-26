#include <Scene/Transform.hpp>
#include <glm.hpp>
#include <gtc\matrix_transform.hpp>
#include <gtc\quaternion.hpp>

// to do implement apply transform ...(private)
// esta madre esta bugeada, TO DO : TERMINAR PRIMERO EL SISTEMA DE TRANSFORM ANTES QUE NADA

Transform::Transform()
{
    this->TransformMatrix = glm::mat4(1.0f);
    this->ModelMatrix = glm::mat4(1.0f);
    this->RotationMatrix = glm::mat4(1.0);
}

Transform::~Transform()
{
}

void Transform::setGlobalPosition(glm::vec3 position)
{
    this->ModelMatrix = glm::mat4(1);
    this->ModelMatrix = glm::translate(this->ModelMatrix, position);
    this->TransformMatrix = ModelMatrix;
}

void Transform::setGlobalRotation(glm::vec3 Axis, float Angle)
{
    this->RotationMatrix = glm::mat4(1);
    RotationMatrix = glm::rotate(RotationMatrix, Angle, Axis);
    TransformMatrix = ModelMatrix * RotationMatrix;
}

void Transform::setLocalPosition(glm::vec3 position)
{
    this->ModelMatrix = glm::translate(this->ModelMatrix, position);
    this->TransformMatrix = ModelMatrix * RotationMatrix;
}

void Transform::setGlobalRotation(glm::vec3 Euler)
{

    RotationMatrix = glm::mat4_cast(glm::quat(glm::vec3(glm::radians(Euler.x), glm::radians(Euler.y), glm::radians(Euler.z))));
    TransformMatrix = ModelMatrix * RotationMatrix;
}

void Transform::setGlobalRotation(glm::quat rotQuat)
{
    RotationMatrix = glm::mat4_cast(rotQuat);
    this->TransformMatrix = ModelMatrix * RotationMatrix;
}

void Transform::translate(glm::vec3 RelativeDirection)
{
    glm::quat tmpRot(RotationMatrix);
    ModelMatrix = glm::translate(this->ModelMatrix, tmpRot * RelativeDirection);
    TransformMatrix = ModelMatrix * RotationMatrix;
}

glm::mat4 Transform::GetTransformMatrix()
{
    return this->TransformMatrix;
}

glm::vec3 Transform::getGlobalPosition()
{
    return ModelMatrix[3];
}

glm::vec3 Transform::getEulerGlobalRotaion()
{
    glm::quat tmpRot(RotationMatrix);
    // return glm::degrees(glm::eulerAngles(tmpRot));

    return glm::vec3(glm::round(glm::degrees(glm::pitch(tmpRot))),
                     glm::round(glm::degrees(glm::yaw(tmpRot))),
                     glm::round(glm::degrees(glm::roll(tmpRot))));
}

glm::quat Transform::getGlobalRotation()
{
    return glm::quat(RotationMatrix);
}
