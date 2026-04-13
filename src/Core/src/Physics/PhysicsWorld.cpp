#include <Physics/PhysicsWorld.hpp>
#include <spdlog/spdlog.h>

// BT_THREADSAFE is defined by Bullet's build system when compiled with
// the multithreading feature (vcpkg: bullet3[multithreading]).
// Guard all MT code paths so we never touch MT APIs on a single-threaded build.
#if BT_THREADSAFE
#include <BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h>
#endif

namespace ettycc
{
    PhysicsWorld::~PhysicsWorld()
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

    void PhysicsWorld::Init()
    {
        // ── 1. Try to enable Bullet's internal multithreading ───────────────
#if BT_THREADSAFE
        taskScheduler_ = btCreateDefaultTaskScheduler();
        if (taskScheduler_ && taskScheduler_->getMaxNumThreads() > 1)
        {
            btSetTaskScheduler(taskScheduler_);
            multithreaded_ = true;
            spdlog::info("[PhysicsWorld] Bullet MT enabled — {} threads",
                         taskScheduler_->getMaxNumThreads());
        }
        else
        {
            if (taskScheduler_) { delete taskScheduler_; taskScheduler_ = nullptr; }
            spdlog::info("[PhysicsWorld] Bullet MT available but scheduler failed — single-threaded");
        }
#else
        spdlog::info("[PhysicsWorld] Single-threaded (rebuild bullet3[multithreading] for MT)");
#endif

        // ── 2. Collision configuration ──────────────────────────────────────
        config_ = std::make_unique<btSoftBodyRigidBodyCollisionConfiguration>();

        // ── 3. Dispatcher ───────────────────────────────────────────────────
#if BT_THREADSAFE
        if (multithreaded_)
            dispatcher_ = std::make_unique<btCollisionDispatcherMt>(config_.get());
        else
#endif
            dispatcher_ = std::make_unique<btCollisionDispatcher>(config_.get());

        broadphase_ = std::make_unique<btDbvtBroadphase>();
        solver_     = std::make_unique<btSequentialImpulseConstraintSolver>();

        // ── 4. World ────────────────────────────────────────────────────────
        world_ = std::make_unique<btSoftRigidDynamicsWorld>(
                     dispatcher_.get(), broadphase_.get(), solver_.get(), config_.get());
        world_->setGravity(btVector3(0.0f, -9.81f, 0.0f));

        btSoftBodyWorldInfo& worldInfo = world_->getWorldInfo();
        worldInfo.m_dispatcher  = dispatcher_.get();
        worldInfo.m_broadphase  = broadphase_.get();
        worldInfo.m_gravity     = btVector3(0.0f, -9.81f, 0.0f);
        worldInfo.air_density   = btScalar(1.2);
        worldInfo.water_density = btScalar(0.0);
        worldInfo.water_offset  = btScalar(0.0);
        worldInfo.water_normal  = btVector3(0.0f, 0.0f, 0.0f);
        worldInfo.m_sparsesdf.Initialize();

        world_->setForceUpdateAllAabbs(false);

        spdlog::info("[PhysicsWorld] initialized (soft+rigid{}) — gravity (0, -9.81, 0)",
                     multithreaded_ ? ", MT dispatcher" : "");
    }

    void PhysicsWorld::SetGravity(const btVector3& g)
    {
        if (world_) world_->setGravity(g);
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        if (!world_) return;
        if (deltaTime <= 0.f || deltaTime > 0.25f)
            deltaTime = 1.f / 60.f;
        world_->stepSimulation(deltaTime, 8);
    }

    btDiscreteDynamicsWorld* PhysicsWorld::GetWorld()
    {
        return world_.get();
    }

    btSoftRigidDynamicsWorld* PhysicsWorld::GetSoftWorld()
    {
        return world_.get();
    }
}
