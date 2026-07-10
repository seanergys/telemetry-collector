#include "config.hpp"

#include <yaml-cpp/yaml.h>

std::vector<
    std::pair<
        std::string,
        std::string>>
load_config(
    const char* filename)
{
    YAML::Node yaml =
        YAML::LoadFile(filename);

    std::vector<
        std::pair<
            std::string,
            std::string>>
        plugins;

    for (const auto& node :
         yaml["plugins"])
    {
        plugins.emplace_back(
            node["library"]
                .as<std::string>(),
            node["config"]
                .as<std::string>());
    }

    return plugins;
}