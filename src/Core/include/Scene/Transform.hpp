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

        // ── Global setters (absolute world-space values) ─────────────────────
        void setGlobalPosition(glm::vec3 position);
        void setGlobalRotation(glm::vec3 Euler);   // degrees
        void setGlobalScale(glm::vec3 scale);

        // ── Global getters ───────────────────────────────────────────────────
        glm::vec3 getGlobalPosition() const;
        glm::quat getGlobalRotation() const;
        glm::vec3 getGlobalScale() const;
        glm::vec3 getEulerGlobalRotation() const;  // extracted from matrix
        glm::vec3 getStoredRotation() const { return rotation_; } // degrees, as stored

        // ── Local setters/getters (no parent hierarchy yet — aliases) ────────
        void setLocalPosition(glm::vec3 position);
        void setLocalRotation(glm::vec3 eulerDegrees);
        void setLocalScale(glm::vec3 scale);
        glm::vec3 getLocalPosition() const;
        glm::vec3 getLocalRotation() const;  // degrees
        glm::vec3 getLocalScale() const;

        // ── Convenience setters ──────────────────────────────────────────────
        // Unified set: rebuilds the full TRS from current stored vectors.
        void set(glm::vec3 eulerDegrees);

        // ── Incremental operations ───────────────────────────────────────────
        void translate(glm::vec3 RelativeDirection);
        void rotate(glm::vec3 eulerDegrees);        // add to current rotation
        void rotate(float angleDegrees, glm::vec3 axis);  // axis-angle

        // ── Direction vectors (from rotation matrix) ─────────────────────────
        glm::vec3 forward() const;   //  Z forward ( 0, 0, 1) rotated
        glm::vec3 right()   const;   //  X right   ( 1, 0, 0) rotated
        glm::vec3 up()      const;   //  Y up      ( 0, 1, 0) rotated

        // ── Look-at ──────────────────────────────────────────────────────────
        void lookAt(glm::vec3 target, glm::vec3 worldUp = glm::vec3(0, 1, 0));

        // ── Matrix operations ────────────────────────────────────────────────
        // Builds a proper T*R*S matrix and stores it without the composition
        // bugs of chaining setGlobalPosition/Rotation/Scale.
        void SetFromTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale);
        void SetMatrix(const glm::mat4 &matrix);
        glm::mat4 GetMatrix() const;

        // Exposes position / rotation / scale to the property visitor.
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