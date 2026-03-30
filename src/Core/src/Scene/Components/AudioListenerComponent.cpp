#include <Scene/Components/AudioListenerComponent.hpp>
#include <Audio/AudioManager.hpp>
#include <Engine.hpp>
#include <Scene/SceneNode.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <AL/al.h>
#include <spdlog/spdlog.h>

namespace ettycc
{
    NodeComponentInfo AudioListenerComponent::GetComponentInfo()
    {
        if (listenerId_ == 0)
            listenerId_ = Utils::GetNextIncrementalId();
        return { listenerId_, componentType, true, ProcessingChannel::AUDIO };
    }

    void AudioListenerComponent::OnStart(std::shared_ptr<Engine> engineInstance)
    {
        audioMgr_ = &engineInstance->audioManager_;

        if (ownerNode_)
            prevPos_ = ownerNode_->transform_.getGlobalPosition();

        if (audioMgr_->IsInitialized())
            alListenerf(AL_GAIN, glm::clamp(gain_, 0.f, 1.f));

        spdlog::info("[AudioListenerComponent] Listener active on node '{}'",
                     ownerNode_ ? ownerNode_->GetName() : "unknown");
    }

    void AudioListenerComponent::OnUpdate(float deltaTime)
    {
        if (!audioMgr_ || !audioMgr_->IsInitialized() || !ownerNode_) return;

        const glm::vec3 pos = ownerNode_->transform_.getGlobalPosition();

        glm::vec3 vel = {0.f, 0.f, 0.f};
        if (deltaTime > 0.0001f)
            vel = (pos - prevPos_) / deltaTime;
        prevPos_ = pos;

        audioMgr_->SetListenerPosition(pos, vel);
        // 2D: always look into screen, Y-up
        audioMgr_->SetListenerOrientation({0.f, 0.f, -1.f}, {0.f, 1.f, 0.f});

        alListenerf(AL_GAIN, glm::clamp(gain_, 0.f, 1.f));
    }

    void AudioListenerComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        Inspect(v);
    }

} // namespace ettycc
