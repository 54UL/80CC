#ifndef SCENE_TRANSFORM_HPP
#define SCENE_TRANSFORM_HPP

#include <glm/glm.hpp>
// EditorPropertyVisitor is forward-declared below so this header stays imgui-free.
namespace ettycc { struct EditorPropertyVisitor; }
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

        // Basic transform operations
        void setGlobalPosition(glm::vec3 position);
        void setGlobalRotation(glm::vec3 Euler);
        void setGlobalScale(glm::vec3 scale);

        glm::vec3 getGlobalPosition() const;
        glm::quat getGlobalRotation() const;
        glm::vec3 getGlobalScale() const;


        void translate(glm::vec3 RelativeDirection);
        // Complex transform operations
        glm::vec3 getEulerGlobalRotation() const;
        glm::vec3 getStoredRotation()     const { return rotation_; } // degrees, as stored by setGlobalRotation

        // Builds a proper T*R*S matrix and stores it without the composition
        // bugs of chaining setGlobalPosition/Rotation/Scale.
        // rot is a unit quaternion; pos and scale are in world units.
        void SetFromTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale);

        void SetMatrix(const glm::mat4 &matrix);
        glm::mat4 GetMatrix() const;

        // Exposes position / rotation / scale to the property visitor.
        // Implemented in Transform.cpp so imgui stays out of this header.
        void Inspect(EditorPropertyVisitor& v);

    // Serialization
    public:
        template <class Archive>
        void save(Archive &ar) const
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

        template <class Archive>
        void load(Archive &ar)
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

            // Rebuild matrices from the loaded vectors.
            // Order matters: position resets modelMatrix, scale builds on it, rotation applies last.
            setGlobalPosition(position_);
            setGlobalScale(scale_);
            setGlobalRotation(rotation_);
        }
    };
}

#endif