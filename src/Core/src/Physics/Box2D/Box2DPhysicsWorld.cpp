#include <Physics/Box2D/Box2DPhysicsWorld.hpp>
#include <Physics/Box2D/Box2DRigidBody.hpp>
#include <Physics/PhysicsConstants.hpp>
#include <spdlog/spdlog.h>

#include <box2d/box2d.h>
#include <cmath>
#include <algorithm>

namespace ettycc::physics
{
    Box2DPhysicsWorld::Box2DPhysicsWorld() = default;
    Box2DPhysicsWorld::~Box2DPhysicsWorld() = default;

    void Box2DPhysicsWorld::Init()
    {
        const float s = kBox2DScale;
        b2Vec2 g(gravity_.x * s, gravity_.y * s);
        world_ = std::make_unique<b2World>(g);

        spdlog::info("[Box2DPhysicsWorld] initialized -- gravity ({:.2f}, {:.2f})  scale={:.4f}",
                     gravity_.x, gravity_.y, s);
    }

    void Box2DPhysicsWorld::Step(float deltaTime)
    {
        if (!world_) return;
        if (deltaTime <= 0.f || deltaTime > kMaxDeltaTime)
            deltaTime = kFallbackDeltaTime;

        world_->Step(deltaTime, 8, 3);
    }

    void Box2DPhysicsWorld::SetGravity(const glm::vec3& g)
    {
        gravity_ = g;
        if (world_)
            world_->SetGravity({g.x * kBox2DScale, g.y * kBox2DScale});
    }

    glm::vec3 Box2DPhysicsWorld::GetGravity() const
    {
        return gravity_;
    }

    std::unique_ptr<IPhysicsBody> Box2DPhysicsWorld::CreateRigidBody(const RigidBodyDef& def)
    {
        if (!world_) return nullptr;

        const float s = kBox2DScale;

        b2BodyDef bodyDef;
        bodyDef.type     = (def.mass > 0.f) ? b2_dynamicBody : b2_staticBody;
        bodyDef.position.Set(def.position.x * s, def.position.y * s);
        bodyDef.angle    = 2.f * std::atan2(def.rotation.z, def.rotation.w);

        b2Body* body = world_->CreateBody(&bodyDef);

        b2FixtureDef fixtureDef;

        // Shape
        b2PolygonShape boxShape;
        b2CircleShape  circleShape;

        switch (def.shape.type)
        {
        case ShapeType::Box:
        {
            float hx = std::max(def.shape.halfExtents.x * s, kMinHalfExtent * s);
            float hy = std::max(def.shape.halfExtents.y * s, kMinHalfExtent * s);
            boxShape.SetAsBox(hx, hy);
            fixtureDef.shape = &boxShape;
            float area = 4.f * hx * hy;
            fixtureDef.density = (def.mass > 0.f && area > 0.f) ? def.mass / area : kDefaultDensity;
            break;
        }
        case ShapeType::Sphere:
        case ShapeType::Circle:
        {
            float r = std::max(def.shape.radius * s, kMinRadius * s);
            circleShape.m_radius = r;
            fixtureDef.shape = &circleShape;
            float area = kPi * r * r;
            fixtureDef.density = (def.mass > 0.f && area > 0.f) ? def.mass / area : kDefaultDensity;
            break;
        }
        default:
        {
            boxShape.SetAsBox(0.5f * s, 0.5f * s);
            fixtureDef.shape = &boxShape;
            fixtureDef.density = kDefaultDensity;
            break;
        }
        }

        fixtureDef.friction    = 0.3f;
        fixtureDef.restitution = 0.1f;
        body->CreateFixture(&fixtureDef);

        if (def.angularFactor.z == 0.f)
            body->SetFixedRotation(true);

        return std::make_unique<Box2DRigidBody>(world_.get(), body, def.mass);
    }

    void Box2DPhysicsWorld::DestroyRigidBody(IPhysicsBody* body)
    {
        (void)body;
    }

    std::unique_ptr<IPhysicsSoftBody> Box2DPhysicsWorld::CreateSoftBody(const SoftBodyDef& /*def*/)
    {
        spdlog::warn("[Box2DPhysicsWorld] soft bodies not supported -- returning nullptr");
        return nullptr;
    }

    void Box2DPhysicsWorld::DestroySoftBody(IPhysicsSoftBody* /*body*/)
    {
    }

} // namespace ettycc::physics
