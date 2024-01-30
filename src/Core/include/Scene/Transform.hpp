#ifndef SCENE_TRANSFORM_HPP
#define SCENE_TRANSFORM_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ettycc
{
    class Transform
    {
    protected:
        glm::mat4 modelMatrix; // underlying matrix...
        bool enabled;
    public:
        Transform();
        ~Transform();

        // operaciones basicas con las transformaciones
        //  transformaciones globales (no son afectadas por un padre)
        void setGlobalPosition(glm::vec3 position);
        void setGlobalRotation(glm::vec3 Axis, float Angle);
        void setGlobalRotation(glm::vec3 Euler);
        // void setGlobalRotation(glm::quat rotQuat);
        void setLocalPosition(glm::vec3 position);
        // void setLocalRotation(glm::vec3 euler);
        void setLocalRotation(glm::quat rotQuat);

        void translate(glm::vec3 RelativeDirection);
        // void setGlobalScale(glm::vec3 scaleFactor);
        // void setLocalScale(glm::vec3 scaleFactor);

        // void setParent(Transform *&father);
        // void unsetParent();

        glm::vec3 getGlobalPosition();
        glm::vec3 getEulerGlobalRotaion();
        glm::quat getGlobalRotation();

        // locals
        // glm::vec3 getLocalPosition();
        // glm::vec3 getEulerLocalRotation();
        // glm::quat getLocalRotation();
        // Utility Functions.
        void SetMatrix(glm::mat4 matrix);
        glm::mat4 GetMatrix();
        // void lookAt(glm::vec3 Target);
        // void lookAt(Transform Target);
    };
}

#endif