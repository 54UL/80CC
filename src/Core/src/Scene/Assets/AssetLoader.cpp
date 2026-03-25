#include <Scene/Assets/AssetLoader.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace ettycc {
    AssetLoader::AssetLoader() {}

    std::size_t AssetLoader::HashFileName(const std::string& filename) const {
        // Use std::hash for simplicity, can be replaced with a stronger hash if needed
        return std::hash<std::string>{}(filename);
    }

    std::string AssetLoader::GetTypeFromPath(const std::filesystem::path& path) const {
        // Infer type from extension or parent folder
        auto ext = path.extension().string();
        if (!ext.empty()) {
            return ext.substr(1); // remove dot
        }
        // fallback: use parent folder name
        return path.parent_path().filename().string();
    }

    void AssetLoader::LoadAssets(const std::string& workingFolder) {
        assets_.clear();
        nameToId_.clear();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(workingFolder)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            std::string name = path.filename().string();
            std::string type = GetTypeFromPath(path);
            std::size_t assetId = HashFileName(name);
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                spdlog::error("Failed to open asset file: {}", path.string());
                continue;
            }
            std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            Asset asset{assetId, name, type, std::move(data)};
            assets_[assetId] = asset;
            nameToId_[name] = assetId;
        }
        spdlog::info("Loaded {} assets from {}", assets_.size(), workingFolder);
    }

    const Asset* AssetLoader::GetAssetById(std::size_t assetId) const {
        auto it = assets_.find(assetId);
        if (it != assets_.end()) return &it->second;
        return nullptr;
    }

    const Asset* AssetLoader::GetAssetByName(const std::string& name) const {
        auto it = nameToId_.find(name);
        if (it != nameToId_.end()) return GetAssetById(it->second);
        return nullptr;
    }

    const std::unordered_map<std::size_t, Asset>& AssetLoader::GetAllAssets() const {
        return assets_;
    }

    std::size_t AssetLoader::GetAssetIdByName(const std::string& name) const {
        auto it = nameToId_.find(name);
        if (it != nameToId_.end()) return it->second;
        return 0;
    }

    std::vector<Asset> AssetLoader::GetLoadedAssets() const {
        std::vector<Asset> result;
        for (const auto& pair : assets_) {
            result.push_back(pair.second);
        }
        return result;
    }
}
