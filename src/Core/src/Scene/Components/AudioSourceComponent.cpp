#include <Scene/Components/AudioSourceComponent.hpp>
#include <Audio/AudioManager.hpp>
#include <Engine.hpp>
#include <Dependencies/Globals.hpp>
#include <Paths.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <spdlog/spdlog.h>
#include <fstream>

// TODO: FIX WIN32 HEADER ISSUES
#undef max
#undef min

namespace ettycc
{
    AudioSourceComponent::~AudioSourceComponent()
    {
        if (audioMgr_ && alSource_ != AL_NONE)
            audioMgr_->DestroySource(alSource_);
    }

    // ── System-facing: initialize AL source ───────────────────────────────────
    void AudioSourceComponent::Init(AudioManager& mgr, Engine& engine,
                                    const Transform& initialTransform)
    {
        engine_   = &engine;
        audioMgr_ = &mgr;

        if (!audioMgr_->IsInitialized())
        {
            spdlog::warn("[AudioSourceComponent] AudioManager not initialized");
            return;
        }

        RebuildSource();

        prevPos_ = initialTransform.getGlobalPosition();

        if (playOnStart_) Play();
    }

    // ── System-facing: per-frame update ───────────────────────────────────────
    void AudioSourceComponent::UpdateAudio(float dt, const Transform& t)
    {
        if (!sourceReady_ || alSource_ == AL_NONE) return;

        if (static_cast<AudioMode>(modeInt_) == AudioMode::Spatial)
            UpdateSpatialPosition(dt, t);

        ApplySourceSettings();
    }

    // ── Private: (re)create AL source ─────────────────────────────────────────
    void AudioSourceComponent::RebuildSource()
    {
        if (alSource_ != AL_NONE)
            audioMgr_->DestroySource(alSource_);
        alSource_    = AL_NONE;
        alBuffer_    = AL_NONE;
        sourceReady_ = false;

        if (clipPath_.empty()) return;

        std::string absPath = clipPath_;
        {
            std::ifstream probe(absPath, std::ios::binary);
            if (!probe.is_open() && engine_)
            {
                const std::string workDir = engine_->globals_->GetWorkingFolder();
                absPath = workDir + paths::AUDIO_DEFAULT + clipPath_;
            }
        }

        alBuffer_ = audioMgr_->LoadBuffer(absPath);
        if (alBuffer_ == AL_NONE)
        {
            spdlog::error("[AudioSourceComponent] Buffer load failed: {}", absPath);
            return;
        }

        alSource_ = audioMgr_->CreateSource();
        if (alSource_ == AL_NONE)
        {
            spdlog::error("[AudioSourceComponent] Source creation failed");
            return;
        }

        alSourcei(alSource_, AL_BUFFER, static_cast<ALint>(alBuffer_));
        ApplySourceSettings();
        sourceReady_ = true;
    }

    // ── Private: push properties to AL ────────────────────────────────────────
    void AudioSourceComponent::ApplySourceSettings()
    {
        if (alSource_ == AL_NONE) return;

        alSourcef(alSource_, AL_GAIN,  glm::clamp(volume_, 0.f, 1.f));
        alSourcef(alSource_, AL_PITCH, glm::clamp(pitch_,  0.1f, 4.f));
        alSourcei(alSource_, AL_LOOPING, loop_ ? AL_TRUE : AL_FALSE);

        const AudioMode mode = static_cast<AudioMode>(modeInt_);

        if (mode == AudioMode::Flat)
        {
            alSourcei (alSource_, AL_SOURCE_RELATIVE, AL_TRUE);
            alSource3f(alSource_, AL_POSITION, 0.f, 0.f, 0.f);
            alSource3f(alSource_, AL_VELOCITY,  0.f, 0.f, 0.f);
            alSourcef (alSource_, AL_ROLLOFF_FACTOR, 0.f);
        }
        else
        {
            alSourcei (alSource_, AL_SOURCE_RELATIVE, AL_FALSE);
            alSourcef (alSource_, AL_REFERENCE_DISTANCE, glm::max(minDistance_, 0.01f));
            alSourcef (alSource_, AL_MAX_DISTANCE,
                       glm::max(maxDistance_, minDistance_ + 0.01f));
            alSourcef (alSource_, AL_ROLLOFF_FACTOR, rolloffFactor_);
        }
    }

    // ── Private: spatial position sync ────────────────────────────────────────
    void AudioSourceComponent::UpdateSpatialPosition(float dt, const Transform& t)
    {
        const glm::vec3 pos = t.getGlobalPosition();

        glm::vec3 vel = {0.f, 0.f, 0.f};
        if (dt > 0.0001f)
            vel = (pos - prevPos_) / dt;
        prevPos_ = pos;

        alSource3f(alSource_, AL_POSITION, pos.x, pos.y, 0.f);
        alSource3f(alSource_, AL_VELOCITY,
                   vel.x * dopplerFactor_, vel.y * dopplerFactor_, 0.f);
    }

    // ── Playback control ──────────────────────────────────────────────────────
    void AudioSourceComponent::Play()
    {
        if (!sourceReady_ || alSource_ == AL_NONE)
        {
            if (audioMgr_ && audioMgr_->IsInitialized()) RebuildSource();
        }
        if (alSource_ != AL_NONE)
        {
            alSourcePlay(alSource_);
            spdlog::info("[AudioSourceComponent] Play: {}", clipPath_);
        }
    }

    void AudioSourceComponent::Stop()
    {
        if (alSource_ != AL_NONE) alSourceStop(alSource_);
    }

    void AudioSourceComponent::Pause()
    {
        if (alSource_ != AL_NONE) alSourcePause(alSource_);
    }

    bool AudioSourceComponent::IsPlaying() const
    {
        if (alSource_ == AL_NONE) return false;
        ALint state;
        alGetSourcei(alSource_, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }

    // ── Editor inspector ──────────────────────────────────────────────────────
    void AudioSourceComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        Inspect(v);

#ifdef EDITOR_BUILD
        ImGui::Spacing();
        ImGui::SeparatorText("Controls");
        if (ImGui::Button("Play"))  Play();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))  Stop();
        ImGui::SameLine();
        if (ImGui::Button("Pause")) Pause();

        if (v.anyChanged && audioMgr_ && audioMgr_->IsInitialized())
            RebuildSource();
#endif
    }

} // namespace ettycc
