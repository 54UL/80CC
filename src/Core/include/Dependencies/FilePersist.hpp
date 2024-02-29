#ifndef UTILS_PATH_MANAGER_HPP
#define UTILS_PATH_MANAGER_HPP

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace ettycc
{
    class FilePersist
    {
    public:
        FilePersist(){}
        ~FilePersist(){}
        
    public:
        void AddValue(const std::string &key, const std::string &value)
        {
            values[key] = value;
        }

        std::string GetValue(const std::string &key) const
        {
            auto it = values.find(key);
            if (it != values.end())
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
                values[el.key()] = el.value();
            }

            file.close();
        }

        void SaveToJson(const std::string &filename) const
        {
            json j;
            for (const auto &pair : values)
            {
                j[pair.first] = pair.second;
            }

            std::ofstream file(filename);
            file << j.dump(4); // pretty-print with 4 spaces
            file.close();
        }

    private:
        std::unordered_map<std::string, std::string> values;
    };
} // namespace ettycc

#endif