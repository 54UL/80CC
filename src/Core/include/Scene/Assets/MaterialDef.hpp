#ifndef MATERIAL_DEF_HPP
#define MATERIAL_DEF_HPP

#include <string>
#include <nlohmann/json.hpp>

namespace ettycc {

    struct MaterialDef
    {
        //TODO: material sho holds multiple shaders, textures, and other properties.  For now we just support one of each for simplicity.
        std::string shader;    // ResourceCache shader base name (e.g. "sprite")
        std::string texture;   // relative path to image (e.g. "images/foo.png")

        static MaterialDef FromJson(const nlohmann::json& j)
        {
            return {
                j.value("shader", "sprite"),
                j.value("texture", "")
            };
        }

        nlohmann::json ToJson() const
        {
            return {
                {"shader", shader},
                {"texture", texture}
            };
        }

        bool IsValid() const { return !shader.empty() && !texture.empty(); }
    };

} // namespace ettycc

#endif // MATERIAL_DEF_HPP
