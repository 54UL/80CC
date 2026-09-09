#pragma once

#include <cstdint>
#include <functional>

namespace ettycc
{
    // Lightweight handle to an asset stored in an AssetPool.
    // 8 bytes total: 4-byte index + 4-byte generation.
    // The generation prevents use-after-free when a slot is recycled.
    template <typename T>
    struct AssetHandle
    {
        uint32_t index      = 0;
        uint32_t generation = 0;

        bool IsValid() const { return generation != 0; }

        static AssetHandle Null() { return {0, 0}; }

        bool operator==(const AssetHandle& o) const { return index == o.index && generation == o.generation; }
        bool operator!=(const AssetHandle& o) const { return !(*this == o); }
        bool operator<(const AssetHandle& o) const
        {
            return index < o.index || (index == o.index && generation < o.generation);
        }
    };

} // namespace ettycc

// Hash support so AssetHandle can be used as an unordered_map key
namespace std
{
    template <typename T>
    struct hash<ettycc::AssetHandle<T>>
    {
        size_t operator()(const ettycc::AssetHandle<T>& h) const noexcept
        {
            // Combine index and generation into a single hash
            size_t seed = std::hash<uint32_t>{}(h.index);
            seed ^= std::hash<uint32_t>{}(h.generation) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}
