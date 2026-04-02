#include <Scene/Components/SoftBodyComponent.hpp>
#include <Engine.hpp>
#include <UI/EditorPropertyVisitor.hpp>

#include <BulletSoftBody/btSoftBodyHelpers.h>

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ettycc
{
    // ── Local disc geometry builder (unchanged from original) ─────────────────
    struct DiscGeometry
    {
        std::vector<float> positions;
        std::vector<float> uvs;
        std::vector<int>   indices;
    };

    static DiscGeometry BuildDisc(float radius, int rings, int sectors)
    {
        DiscGeometry geo;
        const int numVerts = 1 + rings * sectors;
        geo.positions.reserve(numVerts * 3);
        geo.uvs.reserve(numVerts * 2);

        geo.positions.push_back(0.f); geo.positions.push_back(0.f); geo.positions.push_back(0.f);
        geo.uvs.push_back(0.5f); geo.uvs.push_back(0.5f);

        for (int r = 0; r < rings; ++r)
        {
            float t = float(r + 1) / float(rings);
            for (int s = 0; s < sectors; ++s)
            {
                float angle = float(s) * 2.f * float(M_PI) / float(sectors);
                geo.positions.push_back(std::cos(angle) * t * radius);
                geo.positions.push_back(std::sin(angle) * t * radius);
                geo.positions.push_back(0.f);
                geo.uvs.push_back(0.5f + std::cos(angle) * t * 0.5f);
                geo.uvs.push_back(0.5f + std::sin(angle) * t * 0.5f);
            }
        }

        for (int s = 0; s < sectors; ++s)
        {
            geo.indices.push_back(0);
            geo.indices.push_back(1 + s);
            geo.indices.push_back(1 + (s + 1) % sectors);
        }

        for (int r = 0; r < rings - 1; ++r)
        {
            int base0 = 1 + r * sectors;
            int base1 = 1 + (r + 1) * sectors;
            for (int s = 0; s < sectors; ++s)
            {
                int a = base0 + s, b = base0 + (s + 1) % sectors;
                int c = base1 + s, d = base1 + (s + 1) % sectors;
                geo.indices.push_back(a); geo.indices.push_back(c); geo.indices.push_back(b);
                geo.indices.push_back(b); geo.indices.push_back(c); geo.indices.push_back(d);
            }
        }

        return geo;
    }

    // ── Constructor / Destructor ──────────────────────────────────────────────
    SoftBodyComponent::SoftBodyComponent(float radius, glm::vec3 pos, float mass,
                                         std::string texPath)
        : radius_(radius), mass_(mass), initialPosition_(pos), texturePath_(std::move(texPath))
    {}

    SoftBodyComponent::~SoftBodyComponent()
    {
        if (body_ && softWorld_)
        {
            softWorld_->removeSoftBody(body_);
            delete body_;
            body_ = nullptr;
        }
    }

    // ── System-facing: initialize soft body ───────────────────────────────────
    void SoftBodyComponent::InitBody(btSoftRigidDynamicsWorld* world, Engine& engine)
    {
        softWorld_ = world;
        if (!softWorld_)
        {
            spdlog::error("[SoftBodyComponent] soft physics world not initialized");
            return;
        }

        DiscGeometry geo = BuildDisc(radius_, rings_, sectors_);
        const int numVerts     = int(geo.positions.size()) / 3;
        const int numTriangles = int(geo.indices.size())   / 3;

        std::vector<btScalar> btPos;
        btPos.reserve(geo.positions.size());
        for (float f : geo.positions) btPos.push_back(btScalar(f));

        btSoftBodyWorldInfo& worldInfo = softWorld_->getWorldInfo();
        body_ = btSoftBodyHelpers::CreateFromTriMesh(
            worldInfo, btPos.data(), geo.indices.data(), numTriangles, false);

        if (!body_)
        {
            spdlog::error("[SoftBodyComponent] CreateFromTriMesh failed");
            return;
        }

        btSoftBody::Material* mat = body_->m_materials[0];
        mat->m_kLST = btScalar(stiffness_);
        mat->m_kAST = btScalar(stiffness_ * 2.f);
        mat->m_kVST = btScalar(0.0);

        body_->m_cfg.kDP         = btScalar(0.3);    // damping — settles faster, reduces sliding
        body_->m_cfg.kDF         = btScalar(0.8);    // dynamic friction — prevents spinning on surfaces
        body_->m_cfg.kPR         = btScalar(pressure_);
        body_->m_cfg.piterations = 10;
        // CL_RS: cluster-based rigid-soft collision — avoids btSparseSdf::Evaluate
        // which has a floating-point OOB bug when a node lands exactly on a voxel
        // cell boundary (Decompose() can return r.i == CELLSIZE == 3).
        body_->m_cfg.collisions  = btSoftBody::fCollision::CL_RS
                                 | btSoftBody::fCollision::VF_SS;

        body_->generateBendingConstraints(2, mat);
        body_->randomizeConstraints();
        body_->setTotalMass(btScalar(mass_), true);
        body_->getCollisionShape()->setMargin(btScalar(0.02));

        btTransform startXf;
        startXf.setIdentity();
        startXf.setOrigin(btVector3(initialPosition_.x, initialPosition_.y, initialPosition_.z));
        body_->transform(startXf);

        // Break perfect coplanarity BEFORE generating clusters.  Coplanar nodes
        // produce a rank-2 cluster shape matrix, and btMatrix3x3::inverse()
        // asserts det != 0 during CL_RS collision resolution.
        {
            const btScalar zEps = btScalar(0.001);
            for (int i = 0; i < body_->m_nodes.size(); ++i)
                body_->m_nodes[i].m_x.setZ(
                    body_->m_nodes[i].m_x.getZ() + zEps * ((i & 1) ? btScalar(1) : btScalar(-1)));
        }

        // generateClusters is required for CL_RS rigid-soft collision detection.
        // 0 = auto-select cluster count based on mesh topology.
        body_->generateClusters(0);
        softWorld_->addSoftBody(body_);
        lastTrackedCentroid_ = initialPosition_;

        spdlog::info("[SoftBodyComponent] created — radius={:.2f} mass={:.2f} verts={} tris={}",
                     radius_, mass_, numVerts, numTriangles);

        renderable_ = std::make_shared<SoftBodyRenderable>(
            texturePath_, body_, geo.uvs, geo.indices);
        renderable_->Init(GetDependency(Engine));
        engine.renderEngine_.AddRenderable(renderable_);
    }

    // ── System-facing: per-frame update ───────────────────────────────────────
    void SoftBodyComponent::UpdateBody(Transform& t)
    {
        if (!body_) return;

        // Constrain nodes near the 2D plane but allow a tiny Z spread so that
        // cluster shape matrices stay full-rank.  A perfectly coplanar cluster
        // has a rank-2 shape matrix whose inverse triggers btAssert(det != 0)
        // inside btMatrix3x3::inverse() during CL_RS collision resolution.
        const btScalar targetZ = btScalar(initialPosition_.z);
        const btScalar zEps    = btScalar(0.001);   // small spread keeps clusters non-degenerate
        const int nodeCount = body_->m_nodes.size();
        for (int i = 0; i < nodeCount; ++i)
        {
            // Keep each node's Z within ±zEps of the target plane instead of
            // forcing them all to the exact same value.
            btScalar z = body_->m_nodes[i].m_x.getZ();
            if (z < targetZ - zEps || z > targetZ + zEps)
                body_->m_nodes[i].m_x.setZ(targetZ + zEps * ((i & 1) ? btScalar(1) : btScalar(-1)));
            body_->m_nodes[i].m_v.setZ(btScalar(0.0));
        }

        if (nodeCount == 0) return;

        // Compute centroid.
        btVector3 btCentroid(0, 0, 0);
        for (int i = 0; i < nodeCount; ++i)
            btCentroid += body_->m_nodes[i].m_x;
        btCentroid /= btScalar(nodeCount);
        glm::vec3 centroid(btCentroid.getX(), btCentroid.getY(), btCentroid.getZ());

        // Apply any external (editor/gizmo) delta.
        const glm::vec3 nodePos       = t.getGlobalPosition();
        const glm::vec3 externalDelta = nodePos - lastTrackedCentroid_;

        if (glm::length(externalDelta) > 0.001f)
        {
            const btVector3 btDelta(externalDelta.x, externalDelta.y, externalDelta.z);
            for (int i = 0; i < nodeCount; ++i)
            {
                body_->m_nodes[i].m_x += btDelta;
                body_->m_nodes[i].m_q += btDelta;
                body_->m_nodes[i].m_v  = btVector3(0, 0, 0);
            }
            body_->updateBounds();
            centroid += externalDelta;
        }

        t.setGlobalPosition(centroid);
        lastTrackedCentroid_ = centroid;
    }

    // ── Centroid query ────────────────────────────────────────────────────
    glm::vec3 SoftBodyComponent::GetCentroid() const
    {
        if (!body_ || body_->m_nodes.size() == 0)
            return initialPosition_;

        btVector3 sum(0, 0, 0);
        const int n = body_->m_nodes.size();
        for (int i = 0; i < n; ++i)
            sum += body_->m_nodes[i].m_x;
        sum /= btScalar(n);
        return {sum.getX(), sum.getY(), sum.getZ()};
    }

    // ── Editor inspector ──────────────────────────────────────────────────────
    void SoftBodyComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        Inspect(v);
    }

} // namespace ettycc
