#ifndef AUDIO_LISTENER_COMPONENT_HPP
#define AUDIO_LISTENER_COMPONENT_HPP

#include <Scene/NodeComponent.hpp>
#include <Scene/PropertySystem.hpp>
#include <glm/glm.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    // Notes:
    // The position of the owning node drives AL_POSITION each frame.
    // Velocity is computed from frame-to-frame displacement for Doppler.
    class AudioListenerComponent : public NodeComponent
    {
    public:
        static constexpr const char* componentType = "AudioListener";

        AudioListenerComponent() = default;
        ~AudioListenerComponent() override = default;

        NodeComponentInfo GetComponentInfo() override;
        void OnStart(std::shared_ptr<Engine> engineInstance) override;
        void OnUpdate(float deltaTime) override;
        void InspectProperties(EditorPropertyVisitor& v) override;

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
        float    gain_     = 1.0f;
        uint64_t listenerId_ = 0;

        // ── Runtime ───────────────────────────────────────────────────────────
        glm::vec3 prevPos_  = {0.f, 0.f, 0.f};
        class AudioManager* audioMgr_ = nullptr;
    };

} // namespace ettycc
#endif
