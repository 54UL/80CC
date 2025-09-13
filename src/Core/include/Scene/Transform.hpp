#ifndef SCENE_TRANSFORM_HPP
#define SCENE_TRANSFORM_HPP

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cereal/archives/json.hpp>

namespace ettycc
{
    // This is an old as fuck class but gets the work done please be patient...
    class Transform
    {
    protected:

        // TODO: RE-WIRE TRANSORM CALCULATIONS BASED ON THESE VECTORS BELOWS INSTEAD OF HAVING STORED MATRIXES
        glm::vec3 position_;
        glm::vec3 rotation_;
        glm::vec3 scale_;

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
        glm::quat getGlobalRotation() const;

        void SetMatrix(glm::mat4 matrix);
        glm::mat4 GetMatrix();

    // Serialization
    public:
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(
                cereal::make_nvp("position_x", position_.x),
                cereal::make_nvp("position_y", position_.y),
                cereal::make_nvp("position_z", position_.z),

                cereal::make_nvp("rotation_x", rotation_.x),
                cereal::make_nvp("rotation_y", rotation_.y),
                cereal::make_nvp("rotation_z", rotation_.z),

                cereal::make_nvp("scale_x", scale_.x),
                cereal::make_nvp("scale_y", scale_.y),
                cereal::make_nvp("scale_z", scale_.z));
        }
    };
}

#endif