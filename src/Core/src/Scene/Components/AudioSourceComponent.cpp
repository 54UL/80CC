#include <Scene/Components/AudioSourceComponent.hpp>
#include <Audio/AudioManager.hpp>
#include <Engine.hpp>
#include <Scene/SceneNode.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <Dependencies/Globals.hpp>
#include <Paths.hpp>
#include <spdlog/spdlog.h>
#include <fstream>

//TODO: FIX WIN32 HEADER ISSUES
#undef max
#undef min

namespace ettycc
{
    AudioSourceComponent::~AudioSourceComponent()
    {
        if (audioMgr_ && alSource_ != AL_NONE)
            audioMgr_->DestroySource(alSource_);
    }

    NodeComponentInfo AudioSourceComponent::GetComponentInfo()
    {
        if (sourceId_ == 0)
            sourceId_ = Utils::GetNextIncrementalId();
        return { sourceId_, componentType, true, ProcessingChannel::AUDIO };
    }

    void AudioSourceComponent::OnStart(std::shared_ptr<Engine> engineInstance)
    {
        engine_   = engineInstance.get();
        audioMgr_ = &engineInstance->audioManager_;

        if (!audioMgr_->IsInitialized())
        {
            spdlog::warn("[AudioSourceComponent] AudioManager not initialized");
            return;
        }

        RebuildSource();

        if (ownerNode_)
            prevPos_ = ownerNode_->transform_.getGlobalPosition();

        if (playOnStart_)
            Play();
    }

    void AudioSourceComponent::OnUpdate(float deltaTime)
    {
        if (!sourceReady_ || alSource_ == AL_NONE) return;

        // Re-apply settings if the clip path changed at runtime
        if (static_cast<AudioMode>(modeInt_) == AudioMode::Spatial)
            UpdateSpatialPosition(deltaTime);

        // Sync runtime properties every frame so inspector changes take effect
        ApplySourceSettings();
    }

    void AudioSourceComponent::RebuildSource()
    {
        // Clean up old source
        if (alSource_ != AL_NONE)
            audioMgr_->DestroySource(alSource_);
        alSource_    = AL_NONE;
        alBuffer_    = AL_NONE;
        sourceReady_ = false;

        if (clipPath_.empty()) return;

        // Resolve the clip path: try absolute first, then relative to assets/audio/
        std::string absPath = clipPath_;
        {
            std::ifstream probe(absPath, std::ios::binary);
            if (!probe.is_open() && engine_)
            {
                const std::string workDir =
                    engine_->engineResources_->GetWorkingFolder();
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

    void AudioSourceComponent::ApplySourceSettings()
    {
        if (alSource_ == AL_NONE) return;

        alSourcef(alSource_, AL_GAIN,  glm::clamp(volume_, 0.f, 1.f));
        alSourcef(alSource_, AL_PITCH, glm::clamp(pitch_,  0.1f, 4.f));
        alSourcei(alSource_, AL_LOOPING, loop_ ? AL_TRUE : AL_FALSE);

        const AudioMode mode = static_cast<AudioMode>(modeInt_);

        if (mode == AudioMode::Flat)
        {
            // Position-independent: always plays at listener regardless of node position
            alSourcei(alSource_, AL_SOURCE_RELATIVE, AL_TRUE);
            alSource3f(alSource_, AL_POSITION, 0.f, 0.f, 0.f);
            alSource3f(alSource_, AL_VELOCITY, 0.f, 0.f, 0.f);
            alSourcef(alSource_, AL_ROLLOFF_FACTOR, 0.f);
        }
        else
        {
            // Positional: update in UpdateSpatialPosition()
            alSourcei(alSource_, AL_SOURCE_RELATIVE, AL_FALSE);
            alSourcef(alSource_, AL_REFERENCE_DISTANCE, glm::max(minDistance_, 0.01f));
            alSourcef(alSource_, AL_MAX_DISTANCE,       glm::max(maxDistance_, minDistance_ + 0.01f));
            alSourcef(alSource_, AL_ROLLOFF_FACTOR,     rolloffFactor_);
            // AL_DOPPLER_FACTOR is global — we scale the source velocity instead
            // to achieve per-source doppler control (0 = disabled, 1 = full)
        }
    }

    void AudioSourceComponent::UpdateSpatialPosition(float deltaTime)
    {
        if (!ownerNode_) return;

        const glm::vec3 pos = ownerNode_->transform_.getGlobalPosition();

        // Compute velocity for Doppler effect
        glm::vec3 vel = {0.f, 0.f, 0.f};
        if (deltaTime > 0.0001f)
            vel = (pos - prevPos_) / deltaTime;
        prevPos_ = pos;

        alSource3f(alSource_, AL_POSITION, pos.x, pos.y, 0.f); // 2D: z=0
        // Scale velocity by dopplerFactor_ to allow per-source doppler control
        // (0 = no doppler for this source, 1 = full doppler)
        alSource3f(alSource_, AL_VELOCITY,
                   vel.x * dopplerFactor_, vel.y * dopplerFactor_, 0.f);
    }

    // ── Playback control ──────────────────────────────────────────────────────

    void AudioSourceComponent::Play()
    {
        if (!sourceReady_ || alSource_ == AL_NONE)
        {
            // Lazy rebuild: clip path may have been set after construction
            if (audioMgr_ && audioMgr_->IsInitialized())
                RebuildSource();
        }
        if (alSource_ != AL_NONE)
        {
            alSourcePlay(alSource_);
            spdlog::info("[AudioSourceComponent] Play: {}", clipPath_);
        }
    }

    void AudioSourceComponent::Stop()
    {
        if (alSource_ != AL_NONE)
            alSourceStop(alSource_);
    }

    void AudioSourceComponent::Pause()
    {
        if (alSource_ != AL_NONE)
            alSourcePause(alSource_);
    }

    bool AudioSourceComponent::IsPlaying() const
    {
        if (alSource_ == AL_NONE) return false;
        ALint state;
        alGetSourcei(alSource_, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }

    void AudioSourceComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        Inspect(v);

#ifdef EDITOR_BUILD
        // ── Playback buttons ──────────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::SeparatorText("Controls");
        if (ImGui::Button("Play"))  Play();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))  Stop();
        ImGui::SameLine();
        if (ImGui::Button("Pause")) Pause();

        // Rebuild source if clip path changed
        if (v.anyChanged && audioMgr_ && audioMgr_->IsInitialized())
            RebuildSource();
#endif
    }

} // namespace ettycc
