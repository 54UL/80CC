#ifndef PATHS_80CC_HPP
#define PATHS_80CC_HPP

#include <string>

namespace ettycc::paths
{
    const std::string ASSETS_DEFAULT = "../../assets/";
    const std::string CONFIG_DEFAULT = ASSETS_DEFAULT + "config/";
    const std::string RESOURCES_DEFAULT = CONFIG_DEFAULT + "80CC.json";
    const std::string SCENE_DEFAULT = ASSETS_DEFAULT + "scenes/";
} // namespace ettycc

#endif