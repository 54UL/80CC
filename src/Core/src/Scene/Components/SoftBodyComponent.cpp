#include <Scene/Components/SoftBodyComponent.hpp>
#include <Engine.hpp>
#include <Dependency.hpp>
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
    // -------------------------------------------------------------------------
    // Local disc geometry builder
    // -------------------------------------------------------------------------
    struct DiscGeometry
    {
        std::vector<float> positions;   // flat: x,y,z per vertex
        std::vector<float> uvs;         // flat: u,v per vertex
        std::vector<int>   indices;     // triangle list
    };

    static DiscGeometry BuildDisc(float radius, int rings, int sectors)
    {
        DiscGeometry geo;

        const int numVerts = 1 + rings * sectors;
        geo.positions.reserve(numVerts * 3);
        geo.uvs.reserve(numVerts * 2);

        // Center vertex
        geo.positions.push_back(0.0f);
        geo.positions.push_back(0.0f);
        geo.positions.push_back(0.0f);
        geo.uvs.push_back(0.5f);
        geo.uvs.push_back(0.5f);

        // Ring vertices
        for (int r = 0; r < rings; ++r)
        {
            float t = static_cast<float>(r + 1) / static_cast<float>(rings);
            for (int s = 0; s < sectors; ++s)
            {
                float angle = static_cast<float>(s) * 2.0f * static_cast<float>(M_PI) / static_cast<float>(sectors);
                float x = std::cos(angle) * t * radius;
                float y = std::sin(angle) * t * radius;

                geo.positions.push_back(x);
                geo.positions.push_back(y);
                geo.positions.push_back(0.0f);

                geo.uvs.push_back(0.5f + std::cos(angle) * t * 0.5f);
                geo.uvs.push_back(0.5f + std::sin(angle) * t * 0.5f);
            }
        }

        // Center fan triangles
        for (int s = 0; s < sectors; ++s)
        {
            geo.indices.push_back(0);
            geo.indices.push_back(1 + s);
            geo.indices.push_back(1 + (s + 1) % sectors);
        }

        // Ring-to-ring quad triangles
        for (int r = 0; r < rings - 1; ++r)
        {
            int base0 = 1 + r * sectors;
            int base1 = 1 + (r + 1) * sectors;
            for (int s = 0; s < sectors; ++s)
            {
                int a = base0 + s;
                int b = base0 + (s + 1) % sectors;
                int c = base1 + s;
                int d = base1 + (s + 1) % sectors;
                geo.indices.push_back(a);
                geo.indices.push_back(c);
                geo.indices.push_back(b);

                geo.indices.push_back(b);
                geo.indices.push_back(c);
                geo.indices.push_back(d);
            }
        }

        return geo;
    }

    // -------------------------------------------------------------------------
    // Constructor / Destructor
    // -------------------------------------------------------------------------
    SoftBodyComponent::SoftBodyComponent(float radius, glm::vec3 pos, float mass, std::string texPath)
        : radius_(radius)
        , mass_(mass)
        , initialPosition_(pos)
        , texturePath_(std::move(texPath))
    {
        componentId_ = Utils::GetNextIncrementalId();
    }

    SoftBodyComponent::~SoftBodyComponent()
    {
        if (body_ && softWorld_)
        {
            softWorld_->removeSoftBody(body_);
            delete body_;
            body_ = nullptr;
        }
    }

    // -------------------------------------------------------------------------
    // NodeComponent interface
    // -------------------------------------------------------------------------
    NodeComponentInfo SoftBodyComponent::GetComponentInfo()
    {
        return { componentId_, componentType, true, ProcessingChannel::MAIN };
    }

    void SoftBodyComponent::OnStart(std::shared_ptr<Engine> engineInstance)
    {
        softWorld_ = engineInstance->physicsWorld_.GetSoftWorld();
        if (!softWorld_)
        {
            spdlog::error("[SoftBodyComponent] soft physics world not initialized");
            return;
        }

        // Build disc geometry
        DiscGeometry geo = BuildDisc(radius_, rings_, sectors_);

        const int numVerts     = static_cast<int>(geo.positions.size()) / 3;
        const int numTriangles = static_cast<int>(geo.indices.size())   / 3;

        // Bullet wants a flat btScalar positions array
        std::vector<btScalar> btPos;
        btPos.reserve(geo.positions.size());
        for (float f : geo.positions)
            btPos.push_back(static_cast<btScalar>(f));

        // Create the soft body from triangle mesh
        btSoftBodyWorldInfo& worldInfo = softWorld_->getWorldInfo();
        body_ = btSoftBodyHelpers::CreateFromTriMesh(
            worldInfo,
            btPos.data(),
            geo.indices.data(),
            numTriangles,
            false  // randomizeConstraints — we call it manually below
        );

        if (!body_)
        {
            spdlog::error("[SoftBodyComponent] btSoftBodyHelpers::CreateFromTriMesh failed");
            return;
        }

        // Material — plasticine/snot feel: very soft springs, slight area preservation
        btSoftBody::Material* mat = body_->m_materials[0];
        mat->m_kLST = static_cast<btScalar>(stiffness_);             // user-tunable softness
        mat->m_kAST = static_cast<btScalar>(stiffness_ * 2.0f);      // resists collapse but still stretches
        mat->m_kVST = btScalar(0.0);                                  // flat mesh — no volume stiffness

        // Config — flat 2D disc: no pressure or volume conservation (those need a closed 3D volume)
        body_->m_cfg.kDP         = btScalar(0.15);  // damp node velocity — prevents bounce-gap at rigid surfaces
        body_->m_cfg.kDF         = btScalar(0.1);   // low surface friction
        body_->m_cfg.piterations = 10;
        // VF_SS = vertex-face soft-soft collision (no clusters required)
        // SDF_RS is the default rigid-soft mode; leave it as-is
        body_->m_cfg.collisions = btSoftBody::fCollision::SDF_RS
                                | btSoftBody::fCollision::VF_SS;

        // Generate bending constraints for extra structural integrity
        body_->generateBendingConstraints(2, mat);
        body_->randomizeConstraints();

        // Set mass and finalize
        body_->setTotalMass(static_cast<btScalar>(mass_), true);

        // Move to initial world position
        btTransform startXf;
        startXf.setIdentity();
        startXf.setOrigin(btVector3(initialPosition_.x, initialPosition_.y, initialPosition_.z));
        body_->transform(startXf);

        softWorld_->addSoftBody(body_);

        spdlog::info("[SoftBodyComponent] soft body created — radius={:.2f} mass={:.2f} verts={} tris={}",
                     radius_, mass_, numVerts, numTriangles);

        // Build and initialize renderable
        renderable_ = std::make_shared<SoftBodyRenderable>(
            texturePath_,
            body_,
            geo.uvs,
            geo.indices
        );

        renderable_->Init(engineInstance);
        engineInstance->renderEngine_.AddRenderable(renderable_);
    }

    void SoftBodyComponent::OnUpdate(float /*deltaTime*/)
    {
        if (!body_)
            return;

        // Constrain all nodes to the 2D plane (Z = initialPosition_.z)
        const btScalar targetZ = static_cast<btScalar>(initialPosition_.z);
        for (int i = 0; i < body_->m_nodes.size(); ++i)
        {
            body_->m_nodes[i].m_x.setZ(targetZ);
            body_->m_nodes[i].m_v.setZ(btScalar(0.0));
        }
    }

    void SoftBodyComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        Inspect(v);
    }

} // namespace ettycc
