#ifndef AUDIO_SOURCE_COMPONENT_HPP
#define AUDIO_SOURCE_COMPONENT_HPP

#include <Scene/Api.hpp>
#include <Scene/PropertySystem.hpp>
#include <Scene/Transform.hpp>
#include <AL/al.h>
#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <string>

namespace ettycc
{
    struct EditorPropertyVisitor;
    class  AudioManager;
    class  Engine;

    // ── AudioSourceComponent ──────────────────────────────────────────────────
    // Two modes:
    //   Flat    — position-independent 2D sound (music, UI, ambience).
    //   Spatial — 2D positional audio with distance attenuation and Doppler.
    //
    // AudioSystem calls Init() once and UpdateAudio() every AUDIO frame.
    class AudioSourceComponent
    {
    public:
        enum class AudioMode : int { Flat = 0, Spatial = 1 };

        static constexpr const char*        componentType = "AudioSource";
        static constexpr ProcessingChannel  channel       = ProcessingChannel::AUDIO;

        AudioSourceComponent() = default;
        ~AudioSourceComponent();

        // Non-copyable: owns OpenAL source/buffer handles. Movable so
        // std::vector can relocate without double-deleting AL objects.
        AudioSourceComponent(const AudioSourceComponent&)            = delete;
        AudioSourceComponent& operator=(const AudioSourceComponent&) = delete;

        AudioSourceComponent(AudioSourceComponent&& o) noexcept
            : clipPath_(std::move(o.clipPath_))
            , modeInt_(o.modeInt_), volume_(o.volume_), pitch_(o.pitch_)
            , loop_(o.loop_), playOnStart_(o.playOnStart_)
            , minDistance_(o.minDistance_), maxDistance_(o.maxDistance_)
            , dopplerFactor_(o.dopplerFactor_), rolloffFactor_(o.rolloffFactor_)
            , alSource_(o.alSource_), alBuffer_(o.alBuffer_)
            , prevPos_(o.prevPos_), sourceReady_(o.sourceReady_)
            , audioMgr_(o.audioMgr_), engine_(o.engine_)
        {
            o.alSource_    = AL_NONE;
            o.alBuffer_    = AL_NONE;
            o.sourceReady_ = false;
            o.audioMgr_   = nullptr;
            o.engine_     = nullptr;
        }

        AudioSourceComponent& operator=(AudioSourceComponent&& o) noexcept
        {
            if (this == &o) return *this;
            // Release existing AL resources before stealing from o
            if (alSource_ != AL_NONE) { alSourceStop(alSource_); alDeleteSources(1, &alSource_); }
            if (alBuffer_ != AL_NONE) { alDeleteBuffers(1, &alBuffer_); }

            clipPath_      = std::move(o.clipPath_);
            modeInt_       = o.modeInt_;
            volume_        = o.volume_;
            pitch_         = o.pitch_;
            loop_          = o.loop_;
            playOnStart_   = o.playOnStart_;
            minDistance_   = o.minDistance_;
            maxDistance_   = o.maxDistance_;
            dopplerFactor_ = o.dopplerFactor_;
            rolloffFactor_ = o.rolloffFactor_;
            alSource_      = o.alSource_;
            alBuffer_      = o.alBuffer_;
            prevPos_       = o.prevPos_;
            sourceReady_   = o.sourceReady_;
            audioMgr_     = o.audioMgr_;
            engine_       = o.engine_;

            o.alSource_    = AL_NONE;
            o.alBuffer_    = AL_NONE;
            o.sourceReady_ = false;
            o.audioMgr_   = nullptr;
            o.engine_     = nullptr;
            return *this;
        }

        // ── System-facing API (called by AudioSystem) ─────────────────────────
        void Init(AudioManager& mgr, Engine& engine, const Transform& initialTransform);
        void UpdateAudio(float dt, const Transform& t);
        bool IsInitialized() const { return sourceReady_; }

        // ── Playback control (also used by editor inspector buttons) ──────────
        void Play();
        void Stop();
        void Pause();
        bool IsPlaying() const;

        // ── Accessors ─────────────────────────────────────────────────────────
        float     GetMinDistance() const { return minDistance_; }
        float     GetMaxDistance() const { return maxDistance_; }
        AudioMode GetMode()        const { return static_cast<AudioMode>(modeInt_); }

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

        void RebuildSource();
        void ApplySourceSettings();
        void UpdateSpatialPosition(float dt, const Transform& t);

        // ── Serialized fields ─────────────────────────────────────────────────
        std::string clipPath_      = "";
        int         modeInt_       = 0;
        float       volume_        = 1.0f;
        float       pitch_         = 1.0f;
        bool        loop_          = false;
        bool        playOnStart_   = false;
        float       minDistance_   = 1.0f;
        float       maxDistance_   = 10.0f;
        float       dopplerFactor_ = 1.0f;
        float       rolloffFactor_ = 1.0f;

        // ── Runtime (not serialized, set by AudioSystem::Init) ────────────────
        ALuint        alSource_    = AL_NONE;
        ALuint        alBuffer_    = AL_NONE;
        glm::vec3     prevPos_     = {0.f, 0.f, 0.f};
        bool          sourceReady_ = false;
        AudioManager* audioMgr_   = nullptr;
        Engine*       engine_     = nullptr;
    };

} // namespace ettycc
#endif
