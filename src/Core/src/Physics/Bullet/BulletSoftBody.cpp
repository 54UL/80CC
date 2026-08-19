#include <Physics/Bullet/BulletSoftBody.hpp>

namespace ettycc::physics
{
    BulletSoftBody::BulletSoftBody(std::unique_ptr<btSoftBody> body,
                                   btSoftRigidDynamicsWorld* world)
        : body_(std::move(body)), world_(world)
    {}

    BulletSoftBody::~BulletSoftBody()
    {
        if (body_ && world_)
            world_->removeSoftBody(body_.get());
    }

    int BulletSoftBody::GetNodeCount() const
    {
        return body_ ? body_->m_nodes.size() : 0;
    }

    glm::vec3 BulletSoftBody::GetNodePosition(int index) const
    {
        if (!body_ || index < 0 || index >= body_->m_nodes.size())
            return {};
        const btVector3& p = body_->m_nodes[index].m_x;
        return {p.getX(), p.getY(), p.getZ()};
    }

    glm::vec3 BulletSoftBody::GetCentroid() const
    {
        if (!body_ || body_->m_nodes.size() == 0) return {};

        btVector3 sum(0, 0, 0);
        const int n = body_->m_nodes.size();
        for (int i = 0; i < n; ++i)
            sum += body_->m_nodes[i].m_x;
        sum /= btScalar(n);
        return {sum.getX(), sum.getY(), sum.getZ()};
    }

    void BulletSoftBody::Translate(const glm::vec3& delta)
    {
        if (!body_) return;
        const btVector3 btDelta(delta.x, delta.y, delta.z);
        for (int i = 0; i < body_->m_nodes.size(); ++i)
        {
            body_->m_nodes[i].m_x += btDelta;
            body_->m_nodes[i].m_q += btDelta;
        }
    }

    void BulletSoftBody::ZeroVelocities()
    {
        if (!body_) return;
        for (int i = 0; i < body_->m_nodes.size(); ++i)
            body_->m_nodes[i].m_v = btVector3(0, 0, 0);
    }

    void BulletSoftBody::ConstrainToPlane(float z, float epsilon)
    {
        if (!body_) return;
        const btScalar targetZ = btScalar(z);
        const btScalar zEps    = btScalar(epsilon);
        const int nodeCount    = body_->m_nodes.size();
        for (int i = 0; i < nodeCount; ++i)
        {
            btScalar nodeZ = body_->m_nodes[i].m_x.getZ();
            if (nodeZ < targetZ - zEps || nodeZ > targetZ + zEps)
                body_->m_nodes[i].m_x.setZ(
                    targetZ + zEps * ((i & 1) ? btScalar(1) : btScalar(-1)));
            body_->m_nodes[i].m_v.setZ(btScalar(0.0));
        }
    }

    int BulletSoftBody::GetFaceCount() const
    {
        return body_ ? body_->m_faces.size() : 0;
    }

    void BulletSoftBody::GetFaceNodePositions(int faceIdx,
                                               glm::vec3& a,
                                               glm::vec3& b,
                                               glm::vec3& c) const
    {
        if (!body_ || faceIdx < 0 || faceIdx >= body_->m_faces.size()) return;
        const btSoftBody::Face& face = body_->m_faces[faceIdx];
        const btVector3& pa = face.m_n[0]->m_x;
        const btVector3& pb = face.m_n[1]->m_x;
        const btVector3& pc = face.m_n[2]->m_x;
        a = {pa.getX(), pa.getY(), pa.getZ()};
        b = {pb.getX(), pb.getY(), pb.getZ()};
        c = {pc.getX(), pc.getY(), pc.getZ()};
    }

    void BulletSoftBody::UpdateBounds()
    {
        if (body_) body_->updateBounds();
    }

} // namespace ettycc::physics
