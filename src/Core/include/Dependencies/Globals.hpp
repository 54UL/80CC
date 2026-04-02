#pragma once

#include <GlobalKeys.hpp>
#include <Paths.hpp>
#include <iostream>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <spdlog/spdlog.h>

#include <cereal/types/memory.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    // Internal: one group of key→value pairs sharing the same prefix.
    class GlobalGroup
    {
    public:
        std::unordered_map<std::string, std::string> resources;

        template <class Archive>
        void serialize(Archive& ar) { ar(CEREAL_NVP(resources)); }
    };

    // Engine-wide string configuration store.
    // Loads/saves prefix→key→value maps from/to a JSON file.
    // Replaces the old "Resources" class — the name better reflects that
    // this holds global *configuration*, not game assets.
    class Globals
    {
    public:
        Globals()
        {
            const char* root = std::getenv(gk::ENV_ASSETS_ROOT);
            if (root == nullptr)
            {
                spdlog::warn("'{}' not set — using default: {}", gk::ENV_ASSETS_ROOT, paths::ASSETS_DEFAULT);
                SetWorkingFolder(paths::ASSETS_DEFAULT);
            }
            else
            {
                spdlog::info("Assets root: '{}'", root);
                SetWorkingFolder(std::string(root));
            }
        }

        // ── Working folder ─────────────────────────────────────────────────
        // Always normalizes to end with '/' so callers can just concatenate.
        void SetWorkingFolder(const std::string& path)
        {
            workingFolderPath_ = path;
            if (!workingFolderPath_.empty())
            {
                char& last = workingFolderPath_.back();
                if (last == '\\') last = '/';          // backslash → forward slash
                else if (last != '/') workingFolderPath_ += '/';  // add if missing
            }
        }
        const std::string& GetWorkingFolder() const { return workingFolderPath_; }

        void AutoSetWorkingFolder()
        {
            const char* root = std::getenv(gk::ENV_ASSETS_ROOT);
            if (root == nullptr)
            {
                SetWorkingFolder(paths::ASSETS_DEFAULT);
                spdlog::info("'{}' not set — using: {}", gk::ENV_ASSETS_ROOT, paths::ASSETS_DEFAULT);
            }
            else
            {
                SetWorkingFolder(std::string(root));
                spdlog::info("Assets root: '{}'", root);
            }
        }

        // Populate structural defaults. Called before LoadGlobals so the engine
        // keeps working even when the file is missing or corrupt. Values from the
        // file will overwrite these after a successful load.
        void SetupDefaults()
        {
            auto set = [&](const char* prefix, const char* key, const char* val)
            {
                groups_[prefix].resources.emplace(key, val);
            };
            set(gk::prefix::APP,     "title",              "80CC");
            set(gk::prefix::APP,     "flags",              "WINDOWED");
            set(gk::prefix::APP,     "resolution",         "800,600");
            set(gk::prefix::PATHS,   gk::key::PATH_CONFIG,    "config/");
            set(gk::prefix::PATHS,   gk::key::PATH_IMAGES,    "images/");
            set(gk::prefix::PATHS,   gk::key::PATH_SCENES,    "scenes/");
            set(gk::prefix::PATHS,   gk::key::PATH_TEMPLATES, "templates/");
            set(gk::prefix::PATHS,   gk::key::PATH_SHADERS,   "shaders/");
            set(gk::prefix::SPRITES, gk::key::SPRITE_NOT_FOUND, "images/not_found_texture.png");
            set(gk::prefix::STATE,   gk::key::STATE_LAST_SCENE, "");
        }

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(cereal::make_nvp(gk::JSON_ROOT, groups_));
        }

        // ── Key-value access ───────────────────────────────────────────────
        void Set(const std::string& prefix, std::string key, std::string value)
        {
            if (groups_.find(prefix) == groups_.end())
                spdlog::warn("Globals prefix '{}' not found, creating...", prefix);
            groups_[prefix].resources[std::move(key)] = std::move(value);
        }

        std::string Get(const std::string& prefix, const std::string& key)
        {
            auto pit = groups_.find(prefix);
            if (pit == groups_.end())
            {
                spdlog::error("Globals prefix '{}' not loaded", prefix);
                return {};
            }
            auto kit = pit->second.resources.find(key);
            if (kit == pit->second.resources.end())
            {
                spdlog::error("Globals key '{}/{}' not found", prefix, key);
                return {};
            }
            return kit->second;
        }

        // Iterate every prefix/key/value — value passed by ref so the caller can edit it.
        void ForEach(std::function<void(const std::string& prefix,
                                        const std::string& key,
                                        std::string& value)> fn)
        {
            for (auto& [prefix, group] : groups_)
                for (auto& [key, value] : group.resources)
                    fn(prefix, key, value);
        }

    private:
        std::unordered_map<std::string, GlobalGroup> groups_;
        std::string workingFolderPath_;
    };

} // namespace ettycc
