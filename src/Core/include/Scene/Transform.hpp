#ifndef SCENE_TRANSFORM_HPP
#define SCENE_TRANSFORM_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ettycc
{
    // This is an old as fuck class but gets the work done please be patient...
    class Transform
    {
    protected:
        glm::mat4 modelMatrix; // underlying matrix...
        glm::mat4 transformMatrix; 
        glm::mat4 rotationMatrix;

        bool enabled;
    public:
        Transform();
        ~Transform();

        void setGlobalPosition(glm::vec3 position);
        void setGlobalRotation(glm::vec3 Euler);
        void translate(glm::vec3 RelativeDirection);

        glm::vec3 getGlobalPosition();
        glm::vec3 getEulerGlobalRotaion();
        glm::quat getGlobalRotation();

        void SetMatrix(glm::mat4 matrix);
        glm::mat4 GetMatrix();
    };
}

#endif