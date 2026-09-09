#include <Scene/Components/SoftBodyComponent.hpp>
#include <Graphics/Rendering/Entities/SoftBodyRenderable.hpp>
#include <Physics/PhysicsConstants.hpp>
#include <Engine.hpp>
#include <UI/EditorPropertyVisitor.hpp>

#include <Math/Constants.hpp>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

#include <cmath>
#include <vector>
#include <string>

namespace ettycc
{
    // -- Local disc geometry builder -------------------------------------------
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
                float angle = float(s) * math::kTwoPi / float(sectors);
                geo.positions.push_back(glm::cos(angle) * t * radius);
                geo.positions.push_back(glm::sin(angle) * t * radius);
                geo.positions.push_back(0.f);
                geo.uvs.push_back(0.5f + glm::cos(angle) * t * 0.5f);
                geo.uvs.push_back(0.5f + glm::sin(angle) * t * 0.5f);
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

    // -- Constructor / Destructor ----------------------------------------------
    SoftBodyComponent::SoftBodyComponent(float radius, glm::vec3 pos, float mass,
                                         std::string texPath)
        : radius_(radius), mass_(mass), initialPosition_(pos), texturePath_(std::move(texPath))
    {}

    SoftBodyComponent::~SoftBodyComponent() = default;

    void SoftBodyComponent::ReleaseBody()
    {
        // Null out the dangling raw pointer in the renderable before destroying the body.
        if (renderable_)
        {
            auto* sbr = dynamic_cast<SoftBodyRenderable*>(renderable_.get());
            if (sbr) sbr->ClearBody();
        }
        body_.reset();
        renderable_.reset();
    }

    // -- System-facing: initialize soft body -----------------------------------
    void SoftBodyComponent::InitBody(physics::IPhysicsWorld& world, Engine& engine)
    {
        DiscGeometry geo = BuildDisc(radius_, rings_, sectors_);

        physics::SoftBodyDef def;
        def.vertices    = geo.positions;
        def.indices     = geo.indices;
        def.mass        = mass_;
        def.stiffness   = stiffness_;
        def.pressure    = pressure_;
        def.position    = initialPosition_;

        body_ = world.CreateSoftBody(def);

        if (!body_)
        {
            spdlog::error("[SoftBodyComponent] CreateSoftBody failed");
            return;
        }

        const int numVerts = body_->GetNodeCount();
        const int numTriangles = int(geo.indices.size()) / 3;
        lastTrackedCentroid_ = initialPosition_;

        spdlog::info("[SoftBodyComponent] created -- radius={:.2f} mass={:.2f} verts={} tris={}",
                     radius_, mass_, numVerts, numTriangles);

        // Remove stale renderable from a previous init cycle before creating a new one
        if (renderable_)
            engine.renderEngine_.RemoveRenderable(renderable_);

        renderable_ = std::make_shared<SoftBodyRenderable>(
            texturePath_, body_.get(), geo.uvs, geo.indices);
        renderable_->Init(GetDependency(Engine));
        engine.renderEngine_.AddRenderable(renderable_);
    }

    // -- System-facing: per-frame update ---------------------------------------
    void SoftBodyComponent::UpdateBody(Transform& t)
    {
        if (!body_) return;

        // Constrain to 2D plane
        body_->ConstrainToPlane(initialPosition_.z, physics::kSoftBodyPlaneEpsilon);

        if (body_->GetNodeCount() == 0) return;

        glm::vec3 centroid = body_->GetCentroid();

        // Apply any external (editor/gizmo) delta.
        const glm::vec3 nodePos       = t.getGlobalPosition();
        const glm::vec3 externalDelta = nodePos - lastTrackedCentroid_;

        if (glm::length(externalDelta) > 0.001f)
        {
            body_->Translate(externalDelta);
            body_->ZeroVelocities();
            body_->UpdateBounds();
            centroid += externalDelta;
        }

        t.setGlobalPosition(centroid);
        lastTrackedCentroid_ = centroid;

        // Sync vertex data to the renderable's back buffer (lock-free double-buffer).
        // This happens on the main thread after physics step is complete,
        // so the render thread never touches the physics body directly.
        if (renderable_)
        {
            auto* sbr = dynamic_cast<SoftBodyRenderable*>(renderable_.get());
            if (sbr) sbr->SyncFromPhysics();
        }
    }

    // -- Centroid query --------------------------------------------------------
    glm::vec3 SoftBodyComponent::GetCentroid() const
    {
        if (!body_ || body_->GetNodeCount() == 0)
            return initialPosition_;
        return body_->GetCentroid();
    }

    // -- Editor inspector ------------------------------------------------------
    void SoftBodyComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        Inspect(v);
    }

} // namespace ettycc
