#include <Scene/Transform.hpp>
#include <UI/EditorPropertyVisitor.hpp>



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

    // ── Local setters/getters (aliases until parent hierarchy is added) ────────

    void Transform::setLocalPosition(glm::vec3 position) { setGlobalPosition(position); }
    void Transform::setLocalRotation(glm::vec3 eulerDegrees) { setGlobalRotation(eulerDegrees); }
    void Transform::setLocalScale(glm::vec3 scale) { setGlobalScale(scale); }
    glm::vec3 Transform::getLocalPosition() const { return getGlobalPosition(); }
    glm::vec3 Transform::getLocalRotation() const { return rotation_; }
    glm::vec3 Transform::getLocalScale() const { return getGlobalScale(); }

    // ── Convenience ─────────────────────────────────────────────────────────────

    void Transform::set(glm::vec3 eulerDegrees)
    {
        setGlobalRotation(eulerDegrees);
    }

    // ── Incremental operations ──────────────────────────────────────────────────

    void Transform::rotate(glm::vec3 eulerDegrees)
    {
        setGlobalRotation(rotation_ + eulerDegrees);
    }

    void Transform::rotate(float angleDegrees, glm::vec3 axis)
    {
        glm::quat current = glm::quat(glm::radians(rotation_));
        glm::quat delta   = glm::angleAxis(glm::radians(angleDegrees), glm::normalize(axis));
        glm::quat result  = delta * current;
        setGlobalRotation(glm::degrees(glm::eulerAngles(result)));
    }

    // ── Direction vectors ───────────────────────────────────────────────────────

    glm::vec3 Transform::forward() const
    {
        return glm::normalize(glm::vec3(rotationMatrix * glm::vec4(0, 0, 1, 0)));
    }

    glm::vec3 Transform::right() const
    {
        return glm::normalize(glm::vec3(rotationMatrix * glm::vec4(1, 0, 0, 0)));
    }

    glm::vec3 Transform::up() const
    {
        return glm::normalize(glm::vec3(rotationMatrix * glm::vec4(0, 1, 0, 0)));
    }

    // ── Look-at ─────────────────────────────────────────────────────────────────

    void Transform::lookAt(glm::vec3 target, glm::vec3 worldUp)
    {
        glm::vec3 dir = glm::normalize(target - position_);
        // glm::lookAt builds a view matrix (inverted); we want the object rotation.
        glm::mat4 look = glm::inverse(glm::lookAt(position_, target, worldUp));
        glm::quat rot  = glm::quat_cast(look);
        setGlobalRotation(glm::degrees(glm::eulerAngles(rot)));
    }

    // ── TRS ─────────────────────────────────────────────────────────────────────

    void Transform::SetFromTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
    {
        position_  = pos;
        scale_     = scale;
        rotation_  = glm::degrees(glm::eulerAngles(rot));

        rotationMatrix  = glm::mat4_cast(rot);

        // Build canonical T * R * S — rotation is about the object's local origin,
        // then the result is translated to world position.  This avoids the
        // order-dependency bugs in the individual setters.
        modelMatrix     = glm::translate(glm::mat4(1.f), pos)
                        * rotationMatrix
                        * glm::scale(glm::mat4(1.f), scale);
        transformMatrix = modelMatrix;
    }
    void Transform::Inspect(EditorPropertyVisitor& v)
    {
        PROP(position_, "Position");
        PROP(rotation_, "Rotation (deg)");
        PROP(scale_,    "Scale");
    }

} // namespace ettycc

