#ifndef UTILS_PATH_MANAGER_HPP
#define UTILS_PATH_MANAGER_HPP

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace ettycc
{
    class PathManager
    {
    public:
        PathManager(){}
        ~PathManager(){}
        
    public:
        void AddPath(const std::string &key, const std::string &path)
        {
            paths[key] = path;
        }

        std::string GetPath(const std::string &key) const
        {
            auto it = paths.find(key);
            if (it != paths.end())
            {
                return it->second;
            }
            return "";
        }

        void LoadFromJson(const std::string &filename)
        {
            std::ifstream file(filename);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file: " << filename << std::endl;
                return;
            }

            json j;
            file >> j;

            for (auto &el : j.items())
            {
                paths[el.key()] = el.value();
            }

            file.close();
        }

        void SaveToJson(const std::string &filename) const
        {
            json j;
            for (const auto &pair : paths)
            {
                j[pair.first] = pair.second;
            }

            std::ofstream file(filename);
            file << j.dump(4); // pretty-print with 4 spaces
            file.close();
        }

    private:
        std::unordered_map<std::string, std::string> paths;
    };
} // namespace ettycc

#endif