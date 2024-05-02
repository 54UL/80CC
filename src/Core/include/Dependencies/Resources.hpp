#ifndef RESOURCES_HPP
#define RESOURCES_HPP

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <spdlog/spdlog.h>

#include <cereal/types/memory.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    class ResourceDescriptor
    {
    public:
        ResourceDescriptor() {
            resources = std::unordered_map<std::string, std::string>();
        }
        ~ResourceDescriptor() {}

    public:
        std::unordered_map<std::string, std::string> resources;

        // Serialization/Deserialization
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(resources));
        }
    };

    // Resource manager, handles pairs of strings read from the resources.json...
    class Resources
    {

    private:
        std::unordered_map<std::string, ResourceDescriptor> loadedResources_;
        std::string workingFolderPath_;

    public:
        Resources() {
            // Empty initialization...
            loadedResources_ = std::unordered_map<std::string, ResourceDescriptor>();
            workingFolderPath_ = std::string();
        }
        ~Resources() {}

    public:

        auto SetWorkingFolder(const std::string& path) -> void
        {
            workingFolderPath_ = path;
        }

        
        auto GetWorkingFolder() -> const std::string&
        {
            return workingFolderPath_;
        }

        auto Load(const std::string &fileName) -> void
        {
            std::ifstream file(workingFolderPath_ + fileName);

            if (file.is_open())
            {   
                cereal::JSONInputArchive archive(file);
                archive(cereal::make_nvp("engine_data", loadedResources_));
            }
            else 
            {
                spdlog::warn("Resource file does not exist '{}'.", fileName);
            }
        }

        auto Store(const std::string &fileName) -> void
        {
            std::ofstream file(workingFolderPath_ + fileName);

            if (!file.is_open())
            {
                spdlog::info("Cannot store configuration file into '{}'.", workingFolderPath_ + fileName);
            }

            cereal::JSONOutputArchive archive(file);
            archive(cereal::make_nvp("engine_data", loadedResources_));
        }

        auto Set(const std::string &prefix, std::string key, std::string value) -> void
        {
            if (loadedResources_.find(prefix) == loadedResources_.end())
            {
                spdlog::warn("Resource prefix [{}] not found, creating...", prefix);
            }

            auto resourcePack = loadedResources_[prefix].resources[key] = value;
        }

        auto Get(const std::string &prefix, const std::string& key) -> std::string
        {
            if (loadedResources_.find(prefix) == loadedResources_.end())
            {
                spdlog::error("Resource prefix '{}' not loaded", prefix);
                return "";
            }

            auto it = loadedResources_[prefix].resources.find(key);
            if (it != loadedResources_[prefix].resources.end())
            {
                return it->second;
            }
            else
            {
                spdlog::error("Resource with key '{}' not found in prefix '{}'", key, prefix);
                return "";
            }
        }
    };
} // namespace ettycc

#endif
