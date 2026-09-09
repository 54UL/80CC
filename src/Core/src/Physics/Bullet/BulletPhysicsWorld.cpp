#include <Physics/Bullet/BulletPhysicsWorld.hpp>
#include <Physics/Bullet/BulletRigidBody.hpp>
#include <Physics/Bullet/BulletSoftBody.hpp>
#include <Physics/PhysicsConstants.hpp>
#include <spdlog/spdlog.h>

#include <BulletSoftBody/btSoftBodyHelpers.h>

#if BT_THREADSAFE
#include <BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h>
#endif

namespace ettycc::physics
{
    BulletPhysicsWorld::~BulletPhysicsWorld()
    {
        world_.reset();
        solver_.reset();
        broadphase_.reset();
        dispatcher_.reset();
        config_.reset();

#if BT_THREADSAFE
        if (taskScheduler_)
        {
            btSetTaskScheduler(nullptr);
            delete taskScheduler_;
            taskScheduler_ = nullptr;
        }
#endif
    }

    void BulletPhysicsWorld::Init()
    {
#if BT_THREADSAFE
        taskScheduler_ = btCreateDefaultTaskScheduler();
        if (taskScheduler_ && taskScheduler_->getMaxNumThreads() > 1)
        {
            btSetTaskScheduler(taskScheduler_);
            multithreaded_ = true;
            spdlog::info("[BulletPhysicsWorld] MT enabled -- {} threads",
                         taskScheduler_->getMaxNumThreads());
        }
        else
        {
            if (taskScheduler_) { delete taskScheduler_; taskScheduler_ = nullptr; }
            spdlog::info("[BulletPhysicsWorld] MT available but scheduler failed -- single-threaded");
        }
#else
        spdlog::info("[BulletPhysicsWorld] Single-threaded (rebuild bullet3[multithreading] for MT)");
#endif

        config_     = std::make_unique<btSoftBodyRigidBodyCollisionConfiguration>();

#if BT_THREADSAFE
        if (multithreaded_)
            dispatcher_ = std::make_unique<btCollisionDispatcherMt>(config_.get());
        else
#endif
            dispatcher_ = std::make_unique<btCollisionDispatcher>(config_.get());

        broadphase_ = std::make_unique<btDbvtBroadphase>();
        solver_     = std::make_unique<btSequentialImpulseConstraintSolver>();

        world_ = std::make_unique<btSoftRigidDynamicsWorld>(
            dispatcher_.get(), broadphase_.get(), solver_.get(), config_.get());

        const btVector3 defaultG(kDefaultGravityX, kDefaultGravityY, kDefaultGravityZ);
        world_->setGravity(defaultG);

        btSoftBodyWorldInfo& worldInfo = world_->getWorldInfo();
        worldInfo.m_dispatcher  = dispatcher_.get();
        worldInfo.m_broadphase  = broadphase_.get();
        worldInfo.m_gravity     = defaultG;
        worldInfo.air_density   = btScalar(kBulletAirDensity);
        worldInfo.water_density = btScalar(kBulletWaterDensity);
        worldInfo.water_offset  = btScalar(kBulletWaterOffset);
        worldInfo.water_normal  = btVector3(0.0f, 0.0f, 0.0f);
        worldInfo.m_sparsesdf.Initialize();

        world_->setForceUpdateAllAabbs(false);

        spdlog::info("[BulletPhysicsWorld] initialized (soft+rigid{}) -- gravity ({}, {}, {})",
                     multithreaded_ ? ", MT dispatcher" : "",
                     kDefaultGravityX, kDefaultGravityY, kDefaultGravityZ);
    }

    void BulletPhysicsWorld::Step(float deltaTime)
    {
        if (!world_) return;
        if (deltaTime <= 0.f || deltaTime > kMaxDeltaTime)
            deltaTime = kFallbackDeltaTime;
        world_->stepSimulation(deltaTime, kBulletSubSteps);
    }

    void BulletPhysicsWorld::SetGravity(const glm::vec3& g)
    {
        if (!world_) return;
        const btVector3 btg(g.x, g.y, g.z);
        world_->setGravity(btg);
        world_->getWorldInfo().m_gravity = btg;
    }

    glm::vec3 BulletPhysicsWorld::GetGravity() const
    {
        if (!world_) return {};
        const btVector3& g = world_->getGravity();
        return {g.getX(), g.getY(), g.getZ()};
    }

    std::unique_ptr<IPhysicsBody> BulletPhysicsWorld::CreateRigidBody(const RigidBodyDef& def)
    {
        if (!world_) return nullptr;

        const float s = kBulletScale;

        std::unique_ptr<btCollisionShape> shape;
        switch (def.shape.type)
        {
        case ShapeType::Box:
        {
            const auto& h = def.shape.halfExtents;
            const btScalar hx = btScalar(glm::max(h.x * s, kMinHalfExtent));
            const btScalar hy = btScalar(glm::max(h.y * s, kMinHalfExtent));
            const btScalar hz = btScalar(glm::max(h.z * s, kMinHalfExtent));
            shape = std::make_unique<btBoxShape>(btVector3(hx, hy, hz));
            break;
        }
        case ShapeType::Sphere:
        case ShapeType::Circle:
            shape = std::make_unique<btSphereShape>(
                btScalar(glm::max(def.shape.radius * s, kMinRadius)));
            break;
        case ShapeType::Capsule:
            shape = std::make_unique<btCapsuleShape>(
                btScalar(glm::max(def.shape.radius * s, kMinRadius)),
                btScalar(glm::max(def.shape.height * s, kMinHeight)));
            break;
        case ShapeType::ConvexPolygon:
        {
            // Bullet convex hull from 2D polygon vertices (extruded slightly in Z)
            auto convex = std::make_unique<btConvexHullShape>();
            for (auto& v : def.shape.polyVertices)
                convex->addPoint(btVector3(v.x * s, v.y * s, 0.f), false);
            convex->recalcLocalAabb();
            shape = std::move(convex);
            break;
        }
        default:
            shape = std::make_unique<btBoxShape>(btVector3(0.5f, 0.5f, 0.5f));
            break;
        }

        btTransform startXf;
        startXf.setIdentity();
        startXf.setOrigin(btVector3(def.position.x * s, def.position.y * s, def.position.z * s));
        startXf.setRotation(btQuaternion(def.rotation.x, def.rotation.y,
                                          def.rotation.z, def.rotation.w));

        btVector3 localInertia(0.f, 0.f, 0.f);
        if (def.mass > 0.f)
            shape->calculateLocalInertia(def.mass, localInertia);

        auto motionState = std::make_unique<btDefaultMotionState>(startXf);
        btRigidBody::btRigidBodyConstructionInfo ci(
            def.mass, motionState.get(), shape.get(), localInertia);

        auto body = std::make_unique<btRigidBody>(ci);

        if (def.mass > 0.f)
        {
            body->setLinearFactor(btVector3(
                def.linearFactor.x, def.linearFactor.y, def.linearFactor.z));
            body->setAngularFactor(btVector3(
                def.angularFactor.x, def.angularFactor.y, def.angularFactor.z));
        }

        world_->addRigidBody(body.get());

        return std::make_unique<BulletRigidBody>(
            std::move(shape), std::move(motionState), std::move(body), world_.get());
    }

    void BulletPhysicsWorld::DestroyRigidBody(IPhysicsBody* body)
    {
        (void)body;
    }

    std::unique_ptr<IPhysicsSoftBody> BulletPhysicsWorld::CreateSoftBody(const SoftBodyDef& def)
    {
        if (!world_) return nullptr;

        const int numTriangles = int(def.indices.size()) / 3;

        std::vector<btScalar> btPos;
        btPos.reserve(def.vertices.size());
        for (float f : def.vertices) btPos.push_back(btScalar(f));

        btSoftBodyWorldInfo& worldInfo = world_->getWorldInfo();
        btSoftBody* raw = btSoftBodyHelpers::CreateFromTriMesh(
            worldInfo, btPos.data(), def.indices.data(), numTriangles, false);

        if (!raw) return nullptr;

        btSoftBody::Material* mat = raw->m_materials[0];
        mat->m_kLST = btScalar(def.stiffness);
        mat->m_kAST = btScalar(def.stiffness * kSoftBodyASTMultiplier);
        mat->m_kVST = btScalar(kSoftBodyVST);

        raw->m_cfg.kDP         = btScalar(def.damping);
        raw->m_cfg.kDF         = btScalar(def.friction);
        raw->m_cfg.kPR         = btScalar(def.pressure);
        raw->m_cfg.piterations = kSoftBodySolverIterations;
        raw->m_cfg.collisions  = btSoftBody::fCollision::CL_RS
                               | btSoftBody::fCollision::VF_SS;

        raw->generateBendingConstraints(def.bendingDist, mat);
        raw->randomizeConstraints();
        raw->setTotalMass(btScalar(def.mass), true);
        raw->getCollisionShape()->setMargin(btScalar(kSoftBodyCollisionMargin));

        btTransform startXf;
        startXf.setIdentity();
        startXf.setOrigin(btVector3(def.position.x, def.position.y, def.position.z));
        raw->transform(startXf);

        // Break coplanarity before generating clusters.
        {
            const btScalar zEps = btScalar(kSoftBodyCoplanarEpsilon);
            for (int i = 0; i < raw->m_nodes.size(); ++i)
                raw->m_nodes[i].m_x.setZ(
                    raw->m_nodes[i].m_x.getZ() + zEps * ((i & 1) ? btScalar(1) : btScalar(-1)));
        }

        raw->generateClusters(0);

        // Bullet 3.25's btSequentialImpulseConstraintSolver::getOrInitSolverBody
        // asserts isStaticOrKinematicObject() for any collision object that isn't
        // a btRigidBody or a featherstone link.  btSoftBody is none of those, so
        // mark it as kinematic to satisfy the solver's island processing.
        // Actual soft body dynamics are handled separately by
        // btSoftRigidDynamicsWorld::solveSoftBodiesConstraints.
        raw->setCollisionFlags(raw->getCollisionFlags()
                               | btCollisionObject::CF_KINEMATIC_OBJECT);

        world_->addSoftBody(raw);

        return std::make_unique<BulletSoftBody>(
            std::unique_ptr<btSoftBody>(raw), world_.get());
    }

    void BulletPhysicsWorld::DestroySoftBody(IPhysicsSoftBody* body)
    {
        (void)body;
    }

} // namespace ettycc::physics
