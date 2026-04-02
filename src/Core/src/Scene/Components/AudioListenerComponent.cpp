#include <Scene/Components/AudioListenerComponent.hpp>
#include <Audio/AudioManager.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <AL/al.h>
#include <spdlog/spdlog.h>

namespace ettycc
{
    // ── System-facing: initialize AL listener ─────────────────────────────────
    void AudioListenerComponent::Init(AudioManager& mgr, const Transform& initialTransform)
    {
        audioMgr_ = &mgr;
        prevPos_  = initialTransform.getGlobalPosition();

        if (audioMgr_->IsInitialized())
            alListenerf(AL_GAIN, glm::clamp(gain_, 0.f, 1.f));

        spdlog::info("[AudioListenerComponent] Listener initialized");
    }

    // ── System-facing: per-frame update ───────────────────────────────────────
    void AudioListenerComponent::UpdateListener(float dt, const Transform& t)
    {
        if (!audioMgr_ || !audioMgr_->IsInitialized()) return;

        const glm::vec3 pos = t.getGlobalPosition();

        glm::vec3 vel = {0.f, 0.f, 0.f};
        if (dt > 0.0001f)
            vel = (pos - prevPos_) / dt;
        prevPos_ = pos;

        audioMgr_->SetListenerPosition(pos, vel);
        audioMgr_->SetListenerOrientation({0.f, 0.f, -1.f}, {0.f, 1.f, 0.f});
        alListenerf(AL_GAIN, glm::clamp(gain_, 0.f, 1.f));
    }

    // ── Editor inspector ──────────────────────────────────────────────────────
    void AudioListenerComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        Inspect(v);
    }

} // namespace ettycc
