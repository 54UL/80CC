#ifndef PHYSICS_WORLD_HPP
#define PHYSICS_WORLD_HPP

// Legacy header — now just includes the abstract interface and the Bullet
// implementation so existing code that #included PhysicsWorld.hpp still compiles.
// New code should include <Physics/IPhysicsWorld.hpp> directly.
#include <Physics/IPhysicsWorld.hpp>
#include <Physics/PhysicsRegistry.hpp>
#include <Physics/Bullet/BulletPhysicsWorld.hpp>

#endif
