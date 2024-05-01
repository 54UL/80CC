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
        ResourceDescriptor() {}
        ResourceDescriptor(const std::unordered_map<std::string, std::string>& valuesMap) : resources(valuesMap) {}
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
        std::unordered_map<std::string, std::shared_ptr<ResourceDescriptor>> loadedResources_;

    public:
        Resources() {
            // Empty initialization...
            loadedResources_ = std::unordered_map<std::string, std::shared_ptr<ResourceDescriptor>>();
        }
        ~Resources() {}

    public:
        auto Load(const std::string &fileName) -> void
        {
            std::ifstream file(fileName);

            if (!file.is_open())
            {
                spdlog::error("Failed to open file: {}", fileName);
                return;
            }

            cereal::JSONInputArchive archive(file);
            archive(*loadedResources_[fileName]);
        }

        auto Store(const std::string &fileName) -> void
        {
            std::ofstream file(fileName);

            if (!file.is_open())
            {
                spdlog::error("Failed to create file: {}", fileName);
                return;
            }

            cereal::JSONOutputArchive archive(file);
            archive(*loadedResources_[fileName]);
        }

        auto Set(const std::string &fileName, std::string key, std::string value) -> void
        {
            // if (loadedResources_.find(fileName) == loadedResources_.end())
            // {
            //     spdlog::error("Resource file '{}' not loaded", fileName);
            //     return;
            // }
            auto resourcePack = loadedResources_[fileName];
            if (resourcePack)
            {
                resourcePack->resources[key] = value; 
            }
            else 
            {
            //    loadedResources_[fileName] = std::make_shared<ResourceDescriptor>({key, value});
            }
        }

        auto Get(const std::string &fileName, std::string key) -> std::string
        {
            if (loadedResources_.find(fileName) == loadedResources_.end())
            {
                spdlog::error("Resource file '{}' not loaded", fileName);
                return "";
            }

            auto it = loadedResources_[fileName]->resources.find(key);
            if (it != loadedResources_[fileName]->resources.end())
            {
                return it->second;
            }
            else
            {
                spdlog::error("Resource with key '{}' not found in file '{}'", key, fileName);
                return "";
            }
        }
    };
} // namespace ettycc

#endif
