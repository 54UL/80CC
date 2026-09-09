#pragma once

#include "AssetHandle.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <spdlog/spdlog.h>

namespace ettycc
{
    // Contiguous, cache-friendly storage for assets of type T.
    //
    // Assets are stored in a flat std::vector so iterating all assets of a type
    // is a linear memory scan.  Handles carry a generation counter so stale
    // references are detected rather than silently returning wrong data.
    //
    // T must be default-constructible and movable.
    template <typename T>
    class AssetPool
    {
    public:
        // Allocate a new slot and return a handle to it.
        AssetHandle<T> Add(const std::string& path, T&& asset)
        {
            uint32_t idx = static_cast<uint32_t>(assets_.size());
            generations_.push_back(++genCounter_);
            assets_.push_back(std::move(asset));
            paths_.push_back(path);

            AssetHandle<T> h{idx, genCounter_};
            if (!path.empty())
                pathIndex_[path] = h;
            return h;
        }

        // Look up an asset by handle.  Returns nullptr if the handle is stale or null.
        T* Get(AssetHandle<T> h)
        {
            if (!h.IsValid()) return nullptr;
            if (h.index >= assets_.size()) return nullptr;
            if (generations_[h.index] != h.generation) return nullptr;
            return &assets_[h.index];
        }

        const T* Get(AssetHandle<T> h) const
        {
            if (!h.IsValid()) return nullptr;
            if (h.index >= assets_.size()) return nullptr;
            if (generations_[h.index] != h.generation) return nullptr;
            return &assets_[h.index];
        }

        // Look up a handle by path.  Returns a null handle if not found.
        AssetHandle<T> Find(const std::string& path) const
        {
            auto it = pathIndex_.find(path);
            return it != pathIndex_.end() ? it->second : AssetHandle<T>::Null();
        }

        // Direct access to the underlying vector for iteration.
        size_t Size() const { return assets_.size(); }
        T&       operator[](size_t i)       { return assets_[i]; }
        const T& operator[](size_t i) const { return assets_[i]; }

        // Iterate all live assets with their handles.
        template <typename Fn>  // Fn(AssetHandle<T>, T&)
        void ForEach(Fn&& fn)
        {
            for (size_t i = 0; i < assets_.size(); ++i)
            {
                AssetHandle<T> h{static_cast<uint32_t>(i), generations_[i]};
                fn(h, assets_[i]);
            }
        }

        const std::string& GetPath(AssetHandle<T> h) const
        {
            static const std::string kEmpty;
            if (!h.IsValid() || h.index >= paths_.size()) return kEmpty;
            if (generations_[h.index] != h.generation) return kEmpty;
            return paths_[h.index];
        }

        const std::unordered_map<std::string, AssetHandle<T>>& GetPathIndex() const
        {
            return pathIndex_;
        }

    private:
        std::vector<T>          assets_;
        std::vector<uint32_t>   generations_;
        std::vector<std::string> paths_;       // parallel to assets_
        uint32_t                genCounter_ = 0;

        // path -> handle for O(1) lookup
        std::unordered_map<std::string, AssetHandle<T>> pathIndex_;
    };

} // namespace ettycc
