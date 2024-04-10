#ifndef SERIALIZABLE_HPP
#define SERIALIZABLE_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

namespace ettycc
{
    enum class SerializableType
    {
        int32,
        floating,
        string,
        vec3,
        mat4
    };

    struct Serializable
    {
        SerializableType type;
        std::string name;
        void *data;

        Serializable(SerializableType type, const std::string &name, void *data)
            : type(type), name(name), data(data) {}
    };

    static std::vector<uint8_t> Serialize(const std::vector<Serializable>& serializables)
    {
        nlohmann::json json;

        for (const auto& serializable : serializables)
        {
            switch (serializable.type)
            {
            case SerializableType::int32:
                json[serializable.name] = *static_cast<int32_t*>(serializable.data);
                break;
            case SerializableType::floating:
                json[serializable.name] = *static_cast<float*>(serializable.data);
                break;
            case SerializableType::string:
                json[serializable.name] = *static_cast<const char*>(serializable.data);
                break;
            case SerializableType::vec3:
                // json[serializable.name] = glm::to_string(*static_cast<glm::vec3*>(serializable.data));
                break;
            case SerializableType::mat4:
                // json[serializable.name] = glm::to_string(*static_cast<glm::mat4*>(serializable.data));
                break;
            }
        }

        std::vector<uint8_t> data(json.dump().begin(), json.dump().end());
        return data;
    }

    static BinaryContainer Deserialize(const std::vector<uint8_t>& data)
    {
        std::string jsonStr(data.begin(), data.end());
        nlohmann::json json = nlohmann::json::parse(jsonStr);

        BinaryContainer container;
        for (auto it = json.begin(); it != json.end(); ++it)
        {
            container.values[it.key()] = it.value().get<std::string>();
        }

        return container;
    }
}

#endif