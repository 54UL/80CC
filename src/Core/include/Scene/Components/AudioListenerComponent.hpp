#ifndef AUDIO_LISTENER_COMPONENT_HPP
#define AUDIO_LISTENER_COMPONENT_HPP

#include <Scene/Api.hpp>
#include <Scene/PropertySystem.hpp>
#include <Scene/Transform.hpp>
#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    struct EditorPropertyVisitor;
    class  AudioManager;

    // ── AudioListenerComponent ────────────────────────────────────────────────
    // Marks the entity whose position drives the OpenAL listener each frame.
    // AudioSystem calls Init() once and UpdateListener() every AUDIO frame.
    class AudioListenerComponent
    {
    public:
        static constexpr const char*        componentType = "AudioListener";
        static constexpr ProcessingChannel  channel       = ProcessingChannel::AUDIO;

        AudioListenerComponent() = default;
        ~AudioListenerComponent() = default;

        // ── System-facing API (called by AudioSystem) ─────────────────────────
        void Init(AudioManager& mgr, const Transform& initialTransform);
        void UpdateListener(float dt, const Transform& t);
        bool IsInitialized() const { return audioMgr_ != nullptr; }

        // ── Editor inspector ──────────────────────────────────────────────────
        void InspectProperties(EditorPropertyVisitor& v);

        // ── Serialization ─────────────────────────────────────────────────────
        template<class Archive>
        void serialize(Archive& ar)
        {
            CerealVisitor<Archive> cv{ar};
            Inspect(cv);
        }

    private:
        template<typename Visitor>
        void Inspect(Visitor& v)
        {
            PROP(gain_, "Master Gain");
        }

        // ── Serialized ────────────────────────────────────────────────────────
        float gain_ = 1.0f;

        // ── Runtime (not serialized, set by AudioSystem::Init) ────────────────
        glm::vec3     prevPos_ = {0.f, 0.f, 0.f};
        AudioManager* audioMgr_ = nullptr;
    };

} // namespace ettycc
#endif
