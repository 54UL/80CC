#ifndef AUDIO_SOURCE_COMPONENT_HPP
#define AUDIO_SOURCE_COMPONENT_HPP

#include <Scene/NodeComponent.hpp>
#include <Scene/PropertySystem.hpp>
#include <AL/al.h>
#include <glm/glm.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <string>

namespace ettycc
{
    // ── AudioSourceComponent ──────────────────────────────────────────────────
    // Two modes:
    //   Flat    — position-independent 2D sound (music, UI, ambience).
    //             Uses AL_SOURCE_RELATIVE so it always plays at the listener.
    //   Spatial — 2D positional audio with distance attenuation and Doppler.
    //             Position is taken from the owning node's world transform.
    class AudioSourceComponent : public NodeComponent
    {
    public:
        enum class AudioMode : int { Flat = 0, Spatial = 1 };

        static constexpr const char* componentType = "AudioSource";

        AudioSourceComponent() = default;
        ~AudioSourceComponent() override;

        NodeComponentInfo GetComponentInfo() override;
        void OnStart(std::shared_ptr<Engine> engineInstance) override;
        void OnUpdate(float deltaTime) override;
        void InspectProperties(EditorPropertyVisitor& v) override;

        // Runtime playback control (also callable from inspector buttons)
        void Play();
        void Stop();
        void Pause();
        bool IsPlaying() const;

        // Accessors used by the editor gizmo
        float     GetMinDistance() const { return minDistance_; }
        float     GetMaxDistance() const { return maxDistance_; }
        AudioMode GetMode()        const { return static_cast<AudioMode>(modeInt_); }

        // ── Cereal serialization (via shared Inspect template) ────────────────
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
            PROP_SECTION("Audio Clip");
            PROP(clipPath_,      "Clip Path");
            PROP(playOnStart_,   "Play On Start");
            PROP(loop_,          "Loop");
            PROP_SECTION("Playback");
            PROP(volume_,        "Volume");
            PROP(pitch_,         "Pitch");
            PROP_SECTION("Mode");
            PROP(modeInt_,       "Mode (0=Flat 1=Spatial)");
            PROP_SECTION("Spatial");
            PROP(minDistance_,   "Min Distance");
            PROP(maxDistance_,   "Max Distance");
            PROP(dopplerFactor_, "Doppler Factor");
            PROP(rolloffFactor_, "Rolloff Factor");
        }

        void RebuildSource();           // (re)create AL source from current settings
        void ApplySourceSettings();     // push current properties to the AL source
        void UpdateSpatialPosition(float deltaTime); // sync node pos + velocity → AL

        // ── Serialized fields ─────────────────────────────────────────────────
        std::string clipPath_      = "";   // relative to assets/audio/ or absolute
        int         modeInt_       = 0;    // AudioMode cast to int
        float       volume_        = 1.0f;
        float       pitch_         = 1.0f;
        bool        loop_          = false;
        bool        playOnStart_   = false;
        float       minDistance_   = 1.0f;  // inner radius: full volume
        float       maxDistance_   = 10.0f; // outer radius: silent
        float       dopplerFactor_ = 1.0f;  // 0 = no doppler, 1 = realistic
        float       rolloffFactor_ = 1.0f;
        uint64_t    sourceId_      = 0;     // stable component ID

        // ── Runtime (not serialized) ──────────────────────────────────────────
        ALuint    alSource_     = AL_NONE;
        ALuint    alBuffer_     = AL_NONE;
        glm::vec3 prevPos_      = {0.f, 0.f, 0.f};
        bool      sourceReady_  = false;
        class AudioManager* audioMgr_ = nullptr;
        class Engine*       engine_   = nullptr;
    };

} // namespace ettycc
#endif
