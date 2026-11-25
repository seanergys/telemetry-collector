// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <limits>
#include <optional>

#include "plugin.hpp"
#include "config.hpp"

std::optional<telemetry_collector_config_t> load_tc_config(
    const char* filename,
    std::shared_ptr<spdlog::logger> logger)
{
    telemetry_collector_config_t telemetryCollectorConfig;

    try
    {
        YAML::Node yaml = YAML::LoadFile(filename);
        const YAML::Node statusIntervalNode =
            yaml["plugin_status_interval_ms"];
        if (!statusIntervalNode || !statusIntervalNode.IsScalar())
        {
            logger->error(
                "Missing or invalid plugin_status_interval_ms in '{}'",
                filename);
            return std::nullopt;
        }

        const auto intervalMs = statusIntervalNode.as<uint64_t>();
        if (intervalMs > static_cast<uint64_t>(
                             std::numeric_limits<std::chrono::milliseconds::rep>::max()))
        {
            logger->error(
                "plugin_status_interval_ms is too large in '{}'",
                filename);
            return std::nullopt;
        }
        telemetryCollectorConfig.plugin_status_interval =
            std::chrono::milliseconds(
                static_cast<std::chrono::milliseconds::rep>(intervalMs));

        const YAML::Node seanerbusUrlNode = yaml["seanerbus"]["url"];
        if (!seanerbusUrlNode || !seanerbusUrlNode.IsScalar())
        {
            logger->error("Missing or invalid seanerbus.url in '{}'", filename);
            return std::nullopt;
        }

        auto seanerbusUrl = seanerbusUrlNode.as<std::string>();
        if (seanerbusUrl.empty())
        {
            logger->error("seanerbus.url must not be empty in '{}'", filename);
            return std::nullopt;
        }

        constexpr char scheme[] = "seanerbroker://";
        if (seanerbusUrl.rfind(scheme, 0) != 0)
            seanerbusUrl.insert(0, scheme);

        for (const auto& node : yaml["plugins"])
        {
            config_t cfg;
            cfg.seanerbus_url = seanerbusUrl;

            cfg.path = strdup(
                node["path"].as<std::string>().c_str());

            cfg.config = strdup(
                node["config"].as<std::string>().c_str());

            cfg.uuid = strdup(
                node["uuid"].as<std::string>().c_str());

            cfg.name = strdup(
                node["name"].as<std::string>().c_str());

            telemetryCollectorConfig.plugins.push_back(cfg);
        }

        return telemetryCollectorConfig;
    }
    catch (const YAML::Exception& e)
    {
        logger->error("YAML error loading '{}': {}", filename, e.what());
    }
    catch (const std::exception& e)
    {
        logger->error("Error loading '{}': {}", filename, e.what());
    }

    return std::nullopt;
}