#ifndef AUDIO_MANAGER_HPP
#define AUDIO_MANAGER_HPP

#include <AL/al.h>
#include <AL/alc.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ettycc
{
    // ── AudioManager ──────────────────────────────────────────────────────────
    // Owns the OpenAL device and context.  One instance lives on Engine.
    //
    // Two independent APIs live here side by side:
    //
    //  [Component API]  — used by AudioSourceComponent / AudioListenerComponent.
    //                     LoadBuffer / CreateSource / DestroySource / Listener.
    //
    //  [Utility API]    — fire-and-forget sounds with no scene coupling.
    //                     PlayOneShot / PlayTone / PlayStartupChime.
    //                     Manages its own temporary AL sources; cleaned up in Update().
    class AudioManager
    {
    public:
        AudioManager()  = default;
        ~AudioManager() = default;

        void Init();
        void Shutdown();
        void Update(); // ticks one-shot GC — call every frame

        // ── Component API ─────────────────────────────────────────────────────
        // Returns AL_NONE on failure.  Buffer is owned by AudioManager and shared
        // across all component sources that reference the same path.
        ALuint LoadBuffer(const std::string& absoluteFilePath);
        void   UnloadBuffer(const std::string& absoluteFilePath);

        // Components own the returned source and call DestroySource() themselves.
        ALuint CreateSource();
        void   DestroySource(ALuint source);

        void SetListenerPosition(const glm::vec3& pos,
                                 const glm::vec3& velocity = {0.f,0.f,0.f});
        void SetListenerOrientation(const glm::vec3& forward,
                                    const glm::vec3& up);

        bool IsInitialized() const { return context_ != nullptr; }

        // ── Utility / debug sound API ─────────────────────────────────────────
        // Fire-and-forget.  Each call allocates a temporary AL source that is
        // automatically destroyed once playback finishes (polled in Update()).
        // All sounds are flat (AL_SOURCE_RELATIVE) — position-independent.

        // Play a WAV file directly without going through the component system.
        void PlayOneShot(const std::string& absoluteFilePath, float volume = 1.0f);

        // Synthesize and play a pure sine-wave tone.
        //   frequencyHz  — pitch in Hz  (e.g. 440 = A4)
        //   durationSecs — length in seconds
        //   volume       — 0..1 gain
        void PlayTone(float frequencyHz, float durationSecs, float volume = 0.5f);

        // Editor boot sequence: ascending C-E-G major arpeggio chime.
        // Call after Init() when running in editor mode.
        void PlayStartupChime();

    private:
        // ── One-shot source tracking ──────────────────────────────────────────
        struct OneShotSource
        {
            ALuint source    = AL_NONE;
            ALuint buffer    = AL_NONE;
            bool   ownsBuffer = false; // true for synthesized buffers not in cache
        };
        std::vector<OneShotSource> oneShotSources_;

        // Create and play a flat one-shot source; the source + optional ownsBuffer
        // are registered for auto-cleanup in Update().
        void DispatchOneShot(ALuint buffer, float volume, bool ownsBuffer);

        // Generate PCM for a sine wave segment with linear attack/release envelope.
        // Returns 16-bit mono samples at 44100 Hz.
        static std::vector<int16_t> GenerateSinePCM(float frequencyHz,
                                                     float durationSecs,
                                                     float attackSecs  = 0.005f,
                                                     float releaseSecs = 0.010f);

        ALCdevice*  device_  = nullptr;
        ALCcontext* context_ = nullptr;

        std::unordered_map<std::string, ALuint> bufferCache_;
    };

} // namespace ettycc
#endif
