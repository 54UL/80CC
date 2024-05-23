#ifndef PATHS_80CC_HPP
#define PATHS_80CC_HPP

#include <string>

namespace ettycc::paths
{
    const std::string RESOURCES_FILE_NAME =  "80CC.json";
    const std::string ASSETS_DEFAULT = "../../assets/";
    const std::string CONFIG_DEFAULT = ASSETS_DEFAULT + "config/";
    const std::string DEFAULT_SCENE_NAME = "default_scene.json";
    const std::string SCENE_DEFAULT = ASSETS_DEFAULT + "scenes/";
    const std::string RESOURCES_DEFAULT = CONFIG_DEFAULT + RESOURCES_FILE_NAME;
} // namespace ettycc

#endif