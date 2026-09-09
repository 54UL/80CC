#ifndef RENDER_LAYER_CONFIG_HPP
#define RENDER_LAYER_CONFIG_HPP

#include <string>
#include <array>
#include <cstdint>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/array.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace ettycc
{
    // Manages up to 32 named render layers (matching the 32-bit layer/cullingMask
    // bitmask on Renderable and Camera).  Layer 0 is always "Default".
    //
    // Persisted to a JSON file alongside the scene so layer definitions survive
    // editor restarts.
    class RenderLayerConfig
    {
    public:
        static constexpr int kMaxLayers = 32;

        RenderLayerConfig()
        {
            names_.fill("");
            names_[0] = "Default";
            activeCount_ = 1;
        }

        // Layer name access
        const std::string& GetName(int index) const { return names_[index]; }
        void SetName(int index, const std::string& name)
        {
            if (index > 0 && index < kMaxLayers)
                names_[index] = name;
        }

        int GetActiveCount() const { return activeCount_; }

        // Add a new layer. Returns the index, or -1 if full.
        int AddLayer(const std::string& name)
        {
            if (activeCount_ >= kMaxLayers) return -1;
            names_[activeCount_] = name;
            return activeCount_++;
        }

        // Remove a layer by index (cannot remove layer 0 "Default").
        // Shifts higher layers down. Returns true on success.
        bool RemoveLayer(int index)
        {
            if (index <= 0 || index >= activeCount_) return false;
            for (int i = index; i < activeCount_ - 1; ++i)
                names_[i] = names_[i + 1];
            names_[activeCount_ - 1] = "";
            --activeCount_;
            return true;
        }

        // Move layer up/down in the list (reorder).
        void SwapLayers(int a, int b)
        {
            if (a > 0 && b > 0 && a < activeCount_ && b < activeCount_)
                std::swap(names_[a], names_[b]);
        }

        // Bitmask for a layer index
        static uint32_t Bit(int index) { return 1u << index; }

        // Serialize
        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(names_), CEREAL_NVP(activeCount_));
        }

        // File I/O
        bool Save(const std::string& path) const
        {
            try
            {
                std::ofstream os(path);
                if (!os.is_open()) return false;
                cereal::JSONOutputArchive ar(os);
                ar(cereal::make_nvp("layers", const_cast<RenderLayerConfig&>(*this)));
                spdlog::info("[RenderLayerConfig] Saved {} layers to {}", activeCount_, path);
                return true;
            }
            catch (...) { return false; }
        }

        bool Load(const std::string& path)
        {
            try
            {
                std::ifstream is(path);
                if (!is.is_open()) return false;
                cereal::JSONInputArchive ar(is);
                ar(cereal::make_nvp("layers", *this));
                spdlog::info("[RenderLayerConfig] Loaded {} layers from {}", activeCount_, path);
                return true;
            }
            catch (...) { return false; }
        }

    private:
        std::array<std::string, kMaxLayers> names_;
        int activeCount_ = 1;
    };

} // namespace ettycc

#endif
