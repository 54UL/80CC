#ifndef ETTYCC_BULLET_SOFT_BODY_HPP
#define ETTYCC_BULLET_SOFT_BODY_HPP

#include <Physics/IPhysicsSoftBody.hpp>
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <memory>

namespace ettycc::physics
{
    class BulletSoftBody : public IPhysicsSoftBody
    {
    public:
        BulletSoftBody(std::unique_ptr<btSoftBody> body,
                       btSoftRigidDynamicsWorld* world);
        ~BulletSoftBody() override;

        // IPhysicsSoftBody
        int       GetNodeCount()                    const override;
        glm::vec3 GetNodePosition(int index)        const override;
        glm::vec3 GetCentroid()                     const override;

        void Translate(const glm::vec3& delta)             override;
        void ZeroVelocities()                              override;
        void ConstrainToPlane(float z, float epsilon)      override;

        int  GetFaceCount()                         const override;
        void GetFaceNodePositions(int faceIdx,
                                  glm::vec3& a,
                                  glm::vec3& b,
                                  glm::vec3& c)    const override;

        void UpdateBounds()                                override;

    private:
        std::unique_ptr<btSoftBody>  body_;
        btSoftRigidDynamicsWorld*    world_ = nullptr;
    };

} // namespace ettycc::physics

#endif
