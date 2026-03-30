#include <Audio/AudioManager.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

namespace ettycc
{
    // ── Simple PCM WAV loader ─────────────────────────────────────────────────
    namespace
    {
        struct WavData
        {
            std::vector<char> pcm;
            ALenum  format     = AL_NONE;
            ALsizei sampleRate = 0;
        };

        // Read a little-endian value from raw bytes
        template<typename T>
        T readLE(std::ifstream& f)
        {
            T v = 0;
            f.read(reinterpret_cast<char*>(&v), sizeof(T));
            return v;
        }

        WavData LoadWAV(const std::string& path)
        {
            WavData result;
            std::ifstream f(path, std::ios::binary);
            if (!f.is_open())
            {
                spdlog::error("[AudioManager] Cannot open: {}", path);
                return result;
            }

            // RIFF header
            char tag[4];
            f.read(tag, 4);
            if (std::memcmp(tag, "RIFF", 4) != 0)
            {
                spdlog::error("[AudioManager] Not a RIFF file: {}", path);
                return result;
            }
            readLE<uint32_t>(f); // file size field (unused)
            f.read(tag, 4);
            if (std::memcmp(tag, "WAVE", 4) != 0)
            {
                spdlog::error("[AudioManager] Not a WAVE file: {}", path);
                return result;
            }

            uint16_t audioFormat   = 0;
            uint16_t numChannels   = 0;
            uint32_t sampleRate    = 0;
            uint16_t bitsPerSample = 0;
            bool     fmtFound      = false;
            bool     dataFound     = false;

            while (!f.eof() && !dataFound)
            {
                f.read(tag, 4);
                if (f.gcount() < 4) break;
                uint32_t chunkSize = readLE<uint32_t>(f);

                if (std::memcmp(tag, "fmt ", 4) == 0)
                {
                    audioFormat   = readLE<uint16_t>(f);
                    numChannels   = readLE<uint16_t>(f);
                    sampleRate    = readLE<uint32_t>(f);
                    readLE<uint32_t>(f); // byteRate
                    readLE<uint16_t>(f); // blockAlign
                    bitsPerSample = readLE<uint16_t>(f);
                    // skip extra fmt bytes (e.g. extensible format)
                    if (chunkSize > 16)
                        f.seekg(chunkSize - 16, std::ios::cur);
                    fmtFound = true;
                }
                else if (std::memcmp(tag, "data", 4) == 0)
                {
                    if (!fmtFound)
                    {
                        spdlog::error("[AudioManager] data chunk before fmt: {}", path);
                        return result;
                    }
                    result.pcm.resize(chunkSize);
                    f.read(result.pcm.data(), chunkSize);
                    dataFound = true;
                }
                else
                {
                    f.seekg(chunkSize, std::ios::cur); // skip unknown chunks
                }
            }

            if (!fmtFound || !dataFound)
            {
                spdlog::error("[AudioManager] Incomplete WAV (fmt={} data={}): {}",
                              fmtFound, dataFound, path);
                return result;
            }
            if (audioFormat != 1) // PCM only
            {
                spdlog::error("[AudioManager] Only PCM WAV supported (format={}): {}",
                              audioFormat, path);
                return result;
            }

            if      (numChannels == 1 && bitsPerSample == 8)  result.format = AL_FORMAT_MONO8;
            else if (numChannels == 1 && bitsPerSample == 16) result.format = AL_FORMAT_MONO16;
            else if (numChannels == 2 && bitsPerSample == 8)  result.format = AL_FORMAT_STEREO8;
            else if (numChannels == 2 && bitsPerSample == 16) result.format = AL_FORMAT_STEREO16;
            else
            {
                spdlog::error("[AudioManager] Unsupported channels/bits ({}/{}): {}",
                              numChannels, bitsPerSample, path);
                return result;
            }

            result.sampleRate = static_cast<ALsizei>(sampleRate);
            return result;
        }
    } // anonymous namespace

    // ── AudioManager ─────────────────────────────────────────────────────────

    void AudioManager::Init()
    {
        device_ = alcOpenDevice(nullptr);
        if (!device_)
        {
            spdlog::error("[AudioManager] Failed to open OpenAL device");
            return;
        }

        context_ = alcCreateContext(device_, nullptr);
        if (!context_)
        {
            spdlog::error("[AudioManager] Failed to create OpenAL context");
            alcCloseDevice(device_);
            device_ = nullptr;
            return;
        }

        alcMakeContextCurrent(context_);

        // Default listener: at origin, looking into screen (Z-), Y-up
        SetListenerPosition({0.f, 0.f, 0.f});
        SetListenerOrientation({0.f, 0.f, -1.f}, {0.f, 1.f, 0.f});

        // Global doppler settings (components override per-source factor)
        alDopplerFactor(1.0f);
        alSpeedOfSound(343.3f);

        spdlog::info("[AudioManager] OpenAL initialized — device: {}",
                     alcGetString(device_, ALC_DEVICE_SPECIFIER));
    }

    void AudioManager::Shutdown()
    {
        // Force-stop and delete any lingering one-shot sources
        for (auto& s : oneShotSources_)
        {
            if (s.source != AL_NONE) { alSourceStop(s.source); alDeleteSources(1, &s.source); }
            if (s.ownsBuffer && s.buffer != AL_NONE) alDeleteBuffers(1, &s.buffer);
        }
        oneShotSources_.clear();

        for (auto& [path, buf] : bufferCache_)
            alDeleteBuffers(1, &buf);
        bufferCache_.clear();

        if (context_)
        {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(context_);
            context_ = nullptr;
        }
        if (device_)
        {
            alcCloseDevice(device_);
            device_ = nullptr;
        }
        spdlog::info("[AudioManager] Shutdown");
    }

    void AudioManager::Update()
    {
        // Garbage-collect finished one-shot sources each frame
        oneShotSources_.erase(
            std::remove_if(oneShotSources_.begin(), oneShotSources_.end(),
                [](const OneShotSource& s)
                {
                    ALint state = AL_STOPPED;
                    alGetSourcei(s.source, AL_SOURCE_STATE, &state);
                    if (state != AL_STOPPED && state != AL_INITIAL) return false;
                    alDeleteSources(1, &s.source);
                    if (s.ownsBuffer) alDeleteBuffers(1, &s.buffer);
                    return true;
                }),
            oneShotSources_.end());
    }

    ALuint AudioManager::LoadBuffer(const std::string& filePath)
    {
        auto it = bufferCache_.find(filePath);
        if (it != bufferCache_.end())
            return it->second;

        if (!context_)
        {
            spdlog::warn("[AudioManager] Not initialized, cannot load: {}", filePath);
            return AL_NONE;
        }

        WavData wav = LoadWAV(filePath);
        if (wav.format == AL_NONE || wav.pcm.empty())
            return AL_NONE;

        ALuint buf;
        alGenBuffers(1, &buf);
        alBufferData(buf, wav.format, wav.pcm.data(),
                     static_cast<ALsizei>(wav.pcm.size()), wav.sampleRate);

        ALenum err = alGetError();
        if (err != AL_NO_ERROR)
        {
            spdlog::error("[AudioManager] alBufferData error {:#x} for: {}", err, filePath);
            alDeleteBuffers(1, &buf);
            return AL_NONE;
        }

        bufferCache_[filePath] = buf;
        spdlog::info("[AudioManager] Loaded buffer: {}", filePath);
        return buf;
    }

    void AudioManager::UnloadBuffer(const std::string& filePath)
    {
        auto it = bufferCache_.find(filePath);
        if (it == bufferCache_.end()) return;
        alDeleteBuffers(1, &it->second);
        bufferCache_.erase(it);
    }

    ALuint AudioManager::CreateSource()
    {
        if (!context_) return AL_NONE;
        ALuint src;
        alGenSources(1, &src);
        return src;
    }

    void AudioManager::DestroySource(ALuint source)
    {
        if (source == AL_NONE || !context_) return;
        alSourceStop(source);
        alDeleteSources(1, &source);
    }

    void AudioManager::SetListenerPosition(const glm::vec3& pos, const glm::vec3& velocity)
    {
        if (!context_) return;
        alListener3f(AL_POSITION, pos.x, pos.y, pos.z);
        alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }

    void AudioManager::SetListenerOrientation(const glm::vec3& forward, const glm::vec3& up)
    {
        if (!context_) return;
        float ori[6] = { forward.x, forward.y, forward.z,
                         up.x,      up.y,      up.z };
        alListenerfv(AL_ORIENTATION, ori);
    }

    // ── Utility / debug sound API ─────────────────────────────────────────────

    void AudioManager::PlayOneShot(const std::string& absoluteFilePath, float volume)
    {
        if (!context_) return;

        // Reuse the component-side buffer cache — same file = same buffer
        ALuint buf = LoadBuffer(absoluteFilePath);
        if (buf == AL_NONE) return;

        DispatchOneShot(buf, volume, /*ownsBuffer=*/false);
    }

    void AudioManager::PlayTone(float frequencyHz, float durationSecs, float volume)
    {
        if (!context_) return;

        auto pcm = GenerateSinePCM(frequencyHz, durationSecs);

        ALuint buf;
        alGenBuffers(1, &buf);
        alBufferData(buf, AL_FORMAT_MONO16,
                     pcm.data(),
                     static_cast<ALsizei>(pcm.size() * sizeof(int16_t)),
                     44100);

        if (alGetError() != AL_NO_ERROR)
        {
            alDeleteBuffers(1, &buf);
            spdlog::warn("[AudioManager] PlayTone: buffer upload failed");
            return;
        }

        DispatchOneShot(buf, volume, /*ownsBuffer=*/true);
    }

    void AudioManager::PlayStartupChime()
    {
        if (!context_) return;

        // C5-E5-G5 major arpeggio at 44100 Hz, 16-bit mono
        // Each note is generated into one contiguous PCM buffer so the entire
        // chime plays as a single AL source (no timing fuss, no threads).
        constexpr int   SR       = 44100;
        constexpr float NOTE_DUR = 0.12f; // seconds per note
        constexpr float GAP_DUR  = 0.04f; // silence between notes
        constexpr float ATK      = 0.006f;
        constexpr float REL      = 0.012f;

        const float notes[] = { 523.25f, 659.25f, 783.99f }; // C5, E5, G5

        std::vector<int16_t> full;
        full.reserve(static_cast<size_t>((NOTE_DUR + GAP_DUR) * 3 * SR));

        for (float freq : notes)
        {
            auto note = GenerateSinePCM(freq, NOTE_DUR, ATK, REL);
            full.insert(full.end(), note.begin(), note.end());

            // Silence gap
            size_t silenceSamples = static_cast<size_t>(GAP_DUR * SR);
            full.insert(full.end(), silenceSamples, int16_t{0});
        }

        ALuint buf;
        alGenBuffers(1, &buf);
        alBufferData(buf, AL_FORMAT_MONO16,
                     full.data(),
                     static_cast<ALsizei>(full.size() * sizeof(int16_t)),
                     SR);

        if (alGetError() != AL_NO_ERROR)
        {
            alDeleteBuffers(1, &buf);
            spdlog::warn("[AudioManager] PlayStartupChime: buffer upload failed");
            return;
        }

        DispatchOneShot(buf, 0.45f, /*ownsBuffer=*/true);
        spdlog::info("[AudioManager] Startup chime playing");
    }

    // ── Private helpers ───────────────────────────────────────────────────────

    void AudioManager::DispatchOneShot(ALuint buffer, float volume, bool ownsBuffer)
    {
        ALuint src;
        alGenSources(1, &src);

        alSourcei (src, AL_BUFFER,          static_cast<ALint>(buffer));
        alSourcef (src, AL_GAIN,            glm::clamp(volume, 0.f, 1.f));
        alSourcei (src, AL_SOURCE_RELATIVE, AL_TRUE);   // flat — no position
        alSource3f(src, AL_POSITION,        0.f, 0.f, 0.f);
        alSource3f(src, AL_VELOCITY,        0.f, 0.f, 0.f);
        alSourcef (src, AL_ROLLOFF_FACTOR,  0.f);
        alSourcei (src, AL_LOOPING,         AL_FALSE);
        alSourcePlay(src);

        oneShotSources_.push_back({ src, buffer, ownsBuffer });
    }

    std::vector<int16_t> AudioManager::GenerateSinePCM(
        float frequencyHz, float durationSecs,
        float attackSecs,  float releaseSecs)
    {
        constexpr int SR = 44100;
        const size_t  N  = static_cast<size_t>(durationSecs * SR);

        std::vector<int16_t> pcm(N);

        const size_t atkSamples = static_cast<size_t>(attackSecs  * SR);
        const size_t relSamples = static_cast<size_t>(releaseSecs * SR);

        for (size_t i = 0; i < N; ++i)
        {
            // Envelope: linear attack → sustain → linear release
            float env = 1.0f;
            if (i < atkSamples && atkSamples > 0)
                env = static_cast<float>(i) / static_cast<float>(atkSamples);
            else if (i >= N - relSamples && relSamples > 0)
                env = static_cast<float>(N - i) / static_cast<float>(relSamples);

            float t     = static_cast<float>(i) / static_cast<float>(SR);
            float sample = env * std::sin(2.f * static_cast<float>(M_PI) * frequencyHz * t);

            // Scale to int16 range with a little headroom
            pcm[i] = static_cast<int16_t>(sample * 28000.f);
        }

        return pcm;
    }

} // namespace ettycc
