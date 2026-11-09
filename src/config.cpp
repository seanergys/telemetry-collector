// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include <yaml-cpp/yaml.h>
#include <iostream>

#include "plugin.hpp"
#include "config.hpp"

std::vector<config_t> load_tc_config(const char* filename, std::shared_ptr<spdlog::logger> logger)
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
        logger->error("YAML error loading '{}': {}", filename, e.what());
    }
    catch (const std::exception& e)
    {
        logger->error("Error loading '{}': {}", filename, e.what());
    }

    return plugin_configs;
}