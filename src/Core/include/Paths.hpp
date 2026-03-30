#ifndef PATHS_80CC_HPP
#define PATHS_80CC_HPP

#include <string>

// fix: assets default should be a env and the rest should be in a json...
// nosense stuff:

namespace ettycc::paths
{
    // All paths are relative to the assets root (set via ASSETS_80CC env var,
    // or "assets/" relative to the process working directory in standalone mode).
    const std::string ASSETS_DEFAULT    = "assets/";
    const std::string CONFIG_DEFAULT    = "config/";
    const std::string RESOURCES_DEFAULT = "config/80CC.json";
    const std::string SCENE_DEFAULT     = "scenes/";
    const std::string TEMPLATES_DEFAULT  = "templates/";
    const std::string BUILD_CONFIG_FILE  = "config/build_config.json";
    const std::string AUDIO_DEFAULT      = "audio/";
} // namespace ettycc

#endif