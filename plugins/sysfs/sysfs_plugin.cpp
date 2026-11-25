// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <unistd.h>
#include <limits.h>
#include <bits/local_lim.h>

#include <string>
#include <fstream>
#include <system_error>

#include "sysfs_plugin.hpp"
#include "../plugin_common.hpp"

#include "seanerbus/client/connection.hpp"
#include "seanerbus/client/simple_event_loop.hpp"

#include <seanerbus/helpers/resolve.hpp>

static std::map<std::string, SensorReading *> sensor_values;
static Logger logging;
static std::shared_ptr<spdlog::logger> logger;

version_t* get_version (void){
    return nullptr;
}

void *sensor_sense(void *sensor);

// Global sensor values map for the sense function

void *sysfs_create(void)
{
    if (!logger)
        logger = logging.create("telemetry-collector::sysfs");
    // Return a pointer to the plugin_interface_t object as the instance
    return &plugin;
}

bool sysfs_destroy(void *runtime)
{
    // The plugin_interface_t object is statically allocated, so we don't free it
    // Just clean up any resources if needed
    false;
}

void applyFields(const YAML::Node &node, sensor_attribute_t *conf)
{
    plugin_common::applyCommonSensorFields(
        node, conf, plugin_common::UuidPolicy::IgnoreInvalid);
    if (node["path"])
        plugin_common::setCharField(conf->path, node["path"].as<std::string>());
}

bool sysfs_init_sensors(void *runtime)
{
    // Get the plugin config from runtime

    // Connect to the server.
    auto conn = plugin_common::makeConnection(runtime);

    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));

    const size_t totalConfigs = configs.size();
    size_t responses = 0;
    bool uuidMismatch = false;
    for (const auto &entry : configs)
    {
        sensor_attribute_t *conf = entry.second;
        plugin_common::addHostname(conf->nameserver_parts, hostname);

        // generating name str
        std::string nameserver_parts_str =
            plugin_common::nameserverPartsToString(conf->nameserver_parts);

        logger->debug("Resolving sensor {}", nameserver_parts_str);
        plugin_common::resolveName(
            conn, conf->nameserver_parts,
            [conf, &responses, totalConfigs, &uuidMismatch](auto &conn, auto res)
                                    {
            responses++;
            std::string name;
            if (res.ok())
            {
                name = plugin_common::nameserverPartsToString(conf->nameserver_parts);
                std::vector<seanerbus::msg::Entity> entities = res.unwrap_ok().entities();
                if (entities.size() > 0)
                {
                    const std::string resolvedUuid = entities[0].as_string();
                    if (!plugin_common::applyResolvedUuid(*conf, resolvedUuid))
                    {
                        logger->error(
                            "Configured UUID '{}' for sensor {} does not match name server UUID '{}'",
                            conf->uuid,
                            name,
                            resolvedUuid);
                        uuidMismatch = true;
                    }
                    else
                    {
                        const auto configEntry = std::find_if(
                            configs.begin(), configs.end(),
                            [conf](const auto &entry)
                            { return entry.second == conf; });
                        if (configEntry != configs.end())
                            configs.erase(configEntry);
                        configs[conf->uuid] = conf;
                        conf->has_been_resolved = true;
                        logger->debug("Resolved sensor {} successfully to {}", name, conf->uuid);
                    }
                } else {
                    logger->debug("Resolved sensor {} successfully but no entities returned", name);
                }
                if (responses == totalConfigs)
                {
                    logger->debug("All responses received , shutting down connection");
                    conn.shutdown(); // All sensors resolved, shutdown the connection
                }
            } else
            {
                logger->debug("Could not resolve sensor: {}", res.unwrap_error().what());
            } });
    }

    seanerbus::client::SimpleEventLoop sl(conn);
    sl.run();

    if (uuidMismatch)
        return false;

    auto conn2 = plugin_common::makeConnection(runtime);
    responses = 0;
    int to_be_registered = 0;
    for (const auto &entry : configs)
    {
        sensor_attribute_t *conf = entry.second;
        std::string name =
            plugin_common::nameserverPartsToString(conf->nameserver_parts);

        if (conf->has_been_resolved)
        {
            logger->warn("Sensor {} has been resolved, skipping registration", name);
        }
        else
        {
            if (conf->uuid == nullptr || strlen(conf->uuid) == 0)
            {
                plugin_common::setCharField(conf->uuid, plugin_common::generateUuid());
                logger->debug("Generated new UUID {}", conf->uuid);
            }

            to_be_registered++;
            plugin_common::registerSensor(
                conn2, conf->nameserver_parts, conf->uuid,
                [conf, &responses, &to_be_registered, name](
                    seanerbus::client::Connection &conn,
                    seanerbus::Expected<void, seanerbus::Error> res)
                {
                responses++;

                if (!res.ok())
                {
                    logger->debug("Could not register sensor {}: {}", name, res.unwrap_error().what());
                    return;
                }
                logger->debug("Registered sensor {} successfully: {}", name, conf->uuid);
                if (responses == to_be_registered)
                {
                    logger->debug("All responses received , shutting down connection");
                    conn.shutdown(); // All sensors registered, shutdown the connection
                } });
        }
        plugin_common::initializeSensor(
            conf->uuid, conf, sensor_sense, ::sensors, sensor_values);
    }
    if (to_be_registered > 0)
    {
        seanerbus::client::SimpleEventLoop sl2(conn2);
        sl2.run();
    }

    logger->debug("End of sysfs_init_sensors, total sensors configured: {}", ::sensors.size());
    return true; // We're using the global map, not returning a pointer to sensors
}

bool sysfs_finit(void *runtime)
{
    for (const auto &entry : ::configs)
    {
        const sensor_attribute_t *conf = entry.second;
        if (conf)
        {
            delete conf->uuid;
            delete conf->topic;
            delete conf->unit;
            delete conf;
        }
    }

    // Clean up the global sensors map
    for (auto &pair : ::sensors)
    {
        sensor_attribute_t *conf = pair.second->conf;
        if (conf)
        {
            delete conf->uuid;
            delete conf->topic;
            delete conf->unit;
            delete conf;
        }

        sensor_t *sensor = pair.second;
        if (sensor)
            delete sensor;
    }
    ::sensors.clear();
    ::configs.clear();

    for (auto &pair : sensor_values)
    {
        if (pair.second)
            delete pair.second;
    }
    // Clear the sensor_values map as well
    sensor_values.clear();

    return true;
}

void getPayload(sensor_attribute_t *conf, SensorReading *tv)
{
    if (!conf->path || conf->path[0] == '\0')
    {
        logger->error("Sensor {} has no configured file path", conf->uuid);
        return;
    }

    FILE *fp = fopen(conf->path, "r");
    if (fp == nullptr)
    {
        logger->error("Error opening file: {} error: {}", conf->path, strerror(errno));
        return;
    }

    char buf[1024];
    if (fgets(buf, sizeof(buf), fp) == nullptr)
    {
        logger->error("No data read from sensor file: {}", conf->path);
        fclose(fp);
        return;
    }
    fclose(fp);

    // strip trailing newline, if present
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
    {
        buf[len - 1] = '\0';
    }

    const std::string valueStr =
        plugin_common::extractFilteredValue(buf, conf->filter, logger);
    const double value = std::stod(valueStr);
    logger->debug("sensor path {} value {} factor {} offset {}", conf->path, value, conf->factor, conf->offset);

    tv->time = plugin_common::getTimestamp();
    tv->value = plugin_common::applyFactorOffset(value, conf->factor, conf->offset);
}

// Sense function with correct signature
void *sensor_sense(void *sensor)
{
    sensor_t *s = (sensor_t *)sensor;
    sensor_attribute_t *conf = s->conf;

    SensorReading *tv = sensor_values[conf->uuid];

    getPayload(conf, tv);

    // Print the sensor data
    logger->debug("[{}] value={}, timestamp={}", conf->uuid, tv->value, tv->time);
    return tv;
}

void *sysfs_run(void *runtime)
{
    return plugin_common::runSensors(
        ::sensors,
        [](sensor_t *sensor)
        {
            const auto *reading =
                static_cast<SensorReading *>(sensor->sense(sensor));
            return plugin_common::makeTimeValueMessage(
                sensor, reading->time, reading->value);
        },
        runtime,
        logger);
}

void *sysfs_configurator(void *configPtr)
{
    void *result =
        plugin_common::commonConfigurationProcedure<sensor_attribute_t>(
        configPtr, logger, logging, "telemetry-collector::sysfs", applyFields,
        ::configs);

    for (auto sensor = ::configs.begin(); sensor != ::configs.end();)
    {
        const auto *conf = sensor->second;
        std::error_code error;
        const bool pathExists =
            conf->path && conf->path[0] != '\0' &&
            std::filesystem::exists(conf->path, error);
        if (pathExists && !error)
        {
            ++sensor;
            continue;
        }

        logger->warn(
            "Skipping sysfs sensor '{}': configured path is missing or invalid",
            sensor->first);
        auto *invalid = sensor->second;
        delete[] invalid->uuid;
        delete[] invalid->topic;
        delete[] invalid->description;
        delete[] invalid->hostname;
        delete[] invalid->unit;
        delete[] invalid->path;
        delete[] invalid->filter;
        delete invalid;
        sensor = ::configs.erase(sensor);
    }

    return result;
}

// Global plugin instance
plugin_interface_t plugin =
    {
        get_version,
        sysfs_create,
        sysfs_destroy,
        sysfs_init_sensors,
        sysfs_finit,
        sysfs_run,          // run function
        sysfs_configurator, // configurator function
};