#include <yaml-cpp/yaml.h>
#include <iostream>

#include "plugin.hpp"
#include "config.hpp"

std::vector<config_t> load_tc_config(const char* filename)
{
    std::vector<config_t> plugin_configs;

    try
    {
        YAML::Node yaml = YAML::LoadFile(filename);

        for (const auto& node : yaml["plugins"])
        {
            config_t cfg;

            cfg.path = strdup(
                node["path"].as<std::string>().c_str());

            cfg.config = strdup(
                node["config"].as<std::string>().c_str());

            cfg.uuid = strdup(
                node["uuid"].as<std::string>().c_str());

            cfg.name = strdup(
                node["name"].as<std::string>().c_str());

            plugin_configs.push_back(cfg);
        }
    }
    catch (const YAML::Exception& e)
    {
        std::cerr << "YAML error loading '" << filename
                << "': " << e.what() << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error loading '" << filename
                << "': " << e.what() << '\n';
    }

    return plugin_configs;
}