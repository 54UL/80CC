#ifndef EIGHTYCC_HPP
#define EIGHTYCC_HPP

#include <App/App.hpp>
#include <Dependency.hpp>
#include <App/SDL2App.hpp>
#include <App/EnginePipeline.hpp>
#include <Graphics/Rendering.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Scene/Transform.hpp>
#include <Input/PlayerInput.hpp>
#include <UI/DevEditor.hpp>
#include <Engine.hpp>
#include <Dependencies/Resources.hpp>
#include <Paths.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Scene/Components/AudioSourceComponent.hpp>
#include <Scene/Components/AudioListenerComponent.hpp>

// Renderable hierarchy still uses cereal polymorphism (Sprite/Camera are
// stored as shared_ptr<Renderable> inside RenderableNode).
CEREAL_REGISTER_TYPE(ettycc::Sprite);
CEREAL_REGISTER_TYPE(ettycc::Camera);
CEREAL_REGISTER_TYPE(ettycc::Renderable);
CEREAL_REGISTER_POLYMORPHIC_RELATION(ettycc::Renderable, ettycc::Sprite)
CEREAL_REGISTER_POLYMORPHIC_RELATION(ettycc::Renderable, ettycc::Camera)

#endif