#ifndef ASSET_LOADER_HPP
#define ASSET_LOADER_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

namespace ettycc {
    struct Asset {
        std::size_t assetId;
        std::string name;
        std::string type;
        std::vector<char> data;
    };

    class AssetLoader {
    public:
        AssetLoader();
        void LoadAssets(const std::string& workingFolder);
        const Asset* GetAssetById(std::size_t assetId) const;
        const Asset* GetAssetByName(const std::string& name) const;
        const std::unordered_map<std::size_t, Asset>& GetAllAssets() const;
        std::vector<Asset> AssetLoader::GetLoadedAssets() const;

        // Metadata API
        std::size_t GetAssetIdByName(const std::string& name) const;

    private:
        std::unordered_map<std::size_t, Asset> assets_;
        std::unordered_map<std::string, std::size_t> nameToId_;
        std::string GetTypeFromPath(const std::filesystem::path& path) const;
        std::size_t HashFileName(const std::string& filename) const;
    };
}

#endif //ASSET_LOADER_HPP