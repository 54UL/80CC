#pragma once

#include "AssetHandle.hpp"
#include "TextureAsset.hpp"
#include "ShaderAsset.hpp"

#include <string>
#include <variant>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <fstream>
#include <glm/glm.hpp>
#include <GL/glew.h>

namespace ettycc
{
    // A single uniform property value that the material passes to its shader.
    // Supports the most common GLSL types.
    using UniformValue = std::variant<
        float,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        int
    >;

    // A material asset -- the single source of truth for how a surface looks.
    // Multiple sprites reference the same MaterialAsset by handle.
    // Editing a material in the asset view immediately affects all users.
    struct MaterialAsset
    {
        // References into the texture and shader pools
        AssetHandle<TextureAsset> texture = AssetHandle<TextureAsset>::Null();
        AssetHandle<ShaderAsset>  shader  = AssetHandle<ShaderAsset>::Null();

        // Render properties
        glm::vec4 tint          = {1.f, 1.f, 1.f, 1.f};
        float     tilingScale   = 1.f;

        // The relative paths used to resolve handles (persisted)
        std::string texturePath;   // e.g. "images/rock.png" (may be empty for textureless shaders)
        std::string shaderName;    // e.g. "sprite"

        // Shader-specific uniform overrides.
        // Keys are uniform names (e.g. "uBaseColor"), values are typed.
        // Only user-facing uniforms go here; engine-managed ones (PVM, uPV,
        // time, ourTexture, tiling) are set by the renderer and excluded.
        std::unordered_map<std::string, UniformValue> uniforms;

        // ---- GPU uniform upload ------------------------------------------

        void ApplyUniforms(GLuint program) const
        {
            for (const auto& [name, val] : uniforms)
            {
                GLint loc = glGetUniformLocation(program, name.c_str());
                if (loc < 0) continue;

                std::visit([loc](auto&& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, float>)
                        glUniform1f(loc, v);
                    else if constexpr (std::is_same_v<T, glm::vec2>)
                        glUniform2f(loc, v.x, v.y);
                    else if constexpr (std::is_same_v<T, glm::vec3>)
                        glUniform3f(loc, v.x, v.y, v.z);
                    else if constexpr (std::is_same_v<T, glm::vec4>)
                        glUniform4f(loc, v.x, v.y, v.z, v.w);
                    else if constexpr (std::is_same_v<T, int>)
                        glUniform1i(loc, v);
                }, val);
            }
        }

        // ---- Persistence (.material file) ----

        nlohmann::json ToJson() const
        {
            nlohmann::json j = {
                {"shader",      shaderName},
                {"texture",     texturePath},
                {"tint",        {tint.r, tint.g, tint.b, tint.a}},
                {"tilingScale", tilingScale}
            };

            if (!uniforms.empty())
            {
                nlohmann::json uObj = nlohmann::json::object();
                for (const auto& [name, val] : uniforms)
                {
                    std::visit([&uObj, &name](auto&& v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, float>)
                            uObj[name] = { {"type", "float"}, {"value", v} };
                        else if constexpr (std::is_same_v<T, glm::vec2>)
                            uObj[name] = { {"type", "vec2"}, {"value", {v.x, v.y}} };
                        else if constexpr (std::is_same_v<T, glm::vec3>)
                            uObj[name] = { {"type", "vec3"}, {"value", {v.x, v.y, v.z}} };
                        else if constexpr (std::is_same_v<T, glm::vec4>)
                            uObj[name] = { {"type", "vec4"}, {"value", {v.x, v.y, v.z, v.w}} };
                        else if constexpr (std::is_same_v<T, int>)
                            uObj[name] = { {"type", "int"}, {"value", v} };
                    }, val);
                }
                j["uniforms"] = uObj;
            }

            return j;
        }

        void FromJson(const nlohmann::json& j)
        {
            if (j.contains("shader"))      shaderName  = j["shader"].get<std::string>();
            if (j.contains("texture"))     texturePath = j["texture"].get<std::string>();
            if (j.contains("tilingScale")) tilingScale = j["tilingScale"].get<float>();
            if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() == 4)
            {
                tint.r = j["tint"][0].get<float>();
                tint.g = j["tint"][1].get<float>();
                tint.b = j["tint"][2].get<float>();
                tint.a = j["tint"][3].get<float>();
            }

            if (j.contains("uniforms") && j["uniforms"].is_object())
            {
                for (auto& [name, entry] : j["uniforms"].items())
                {
                    if (!entry.contains("type") || !entry.contains("value")) continue;
                    std::string type = entry["type"].get<std::string>();
                    const auto& v = entry["value"];

                    if (type == "float")
                        uniforms[name] = v.get<float>();
                    else if (type == "int")
                        uniforms[name] = v.get<int>();
                    else if (type == "vec2" && v.is_array() && v.size() >= 2)
                        uniforms[name] = glm::vec2(v[0].get<float>(), v[1].get<float>());
                    else if (type == "vec3" && v.is_array() && v.size() >= 3)
                        uniforms[name] = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
                    else if (type == "vec4" && v.is_array() && v.size() >= 4)
                        uniforms[name] = glm::vec4(v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), v[3].get<float>());
                }
            }
        }

        bool Save(const std::string& path) const
        {
            try {
                std::ofstream os(path);
                if (!os.is_open()) return false;
                os << ToJson().dump(4);
                return true;
            } catch (...) { return false; }
        }

        bool Load(const std::string& path)
        {
            try {
                std::ifstream is(path);
                if (!is.is_open()) return false;
                nlohmann::json j;
                is >> j;
                FromJson(j);
                return true;
            } catch (...) { return false; }
        }

        // Valid if we have a shader. Texture is optional (procedural shaders don't need one).
        bool IsValid() const { return !shaderName.empty(); }
    };

} // namespace ettycc
