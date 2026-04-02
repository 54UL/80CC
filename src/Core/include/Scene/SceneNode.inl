// SceneNode.inl — template implementations that need Scene fully defined.
// Included at the bottom of Scene.hpp, never included directly.
#pragma once

namespace ettycc {

template<typename T>
void SceneNode::AddComponent(T comp)
{
    if (scene_)
    {
        scene_->registry_.Add<T>(id_, std::move(comp));
    }
    else
    {
        // Queue for later — flushed into the registry inside SceneNode::AddNode().
        // Wrap in shared_ptr so the lambda is copy-constructible (required by std::function)
        // even when T is move-only.
        auto shared = std::make_shared<T>(std::move(comp));
        pendingComponents_.push_back(
            [shared](ecs::Registry& reg, ecs::Entity e) {
                if (!reg.Has<T>(e))
                    reg.Add<T>(e, std::move(*shared));
            });
    }
}

template<typename T>
T* SceneNode::GetComponent()
{
    if (!scene_) return nullptr;
    return scene_->registry_.Get<T>(id_);
}

template<typename T>
bool SceneNode::HasComponent() const
{
    if (!scene_) return false;
    return scene_->registry_.Has<T>(id_);
}

} // namespace ettycc
