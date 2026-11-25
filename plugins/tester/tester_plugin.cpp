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
#include <map>
#include <unistd.h>
#include <limits.h>
#include <bits/local_lim.h>

#include <string>
#include <fstream>
#include <regex>

#include "tester_plugin.hpp"
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

void *tester_create(void)
{
    if (!logger)
        logger = logging.create("telemetry-collector::tester");
    return &plugin;
}

bool tester_destroy(void *runtime)
{
    // The plugin_interface_t object is statically allocated, so we don't free it
    // Just clean up any resources if needed
    false;
}

void applyFields(const YAML::Node &node, sensor_attribute_t *conf)
{
    plugin_common::applyCommonSensorFields(
        node,
        conf,
        plugin_common::UuidPolicy::GenerateWhenInvalid,
        {.factor_offset = false, .topic_filter = false});
}

bool tester_init_sensors(void *runtime)
{
    // Get the plugin config from runtime

    // Connect to the server.
    auto conn = plugin_common::makeConnection(runtime);

    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));

    for (const auto &[uuid, conf] : configs)
    {
        plugin_common::initializeSensor(
            uuid, conf, sensor_sense, ::sensors, sensor_values);
        plugin_common::addHostname(conf->nameserver_parts, hostname);

        const auto nameserver_parts_str =
            plugin_common::nameserverPartsToString(conf->nameserver_parts);
        logger->info("Registering sensor {{}}: {}", nameserver_parts_str, conf->uuid);
        plugin_common::registerSensor(conn, conf->nameserver_parts, conf->uuid);
    }

    seanerbus::client::SimpleEventLoop sl(conn);
    sl.run();

    conn.shutdown();
    return true; // We're using the global map, not returning a pointer to sensors
}

bool tester_finit(void *runtime)
{
    for (auto &pair : ::configs)
    {
        sensor_attribute_t *conf = pair.second;
        if (conf)
        {
            delete conf->uuid;
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

// Sense function with correct signature
void *sensor_sense(void *sensor)
{
    sensor_t *s = (sensor_t *)sensor;
    sensor_attribute_t *conf = s->conf;

    SensorReading *tv = sensor_values[conf->uuid];
    tv->time = plugin_common::getTimestamp();
    tv->value++;

    // Print the sensor data
    logger->debug("[{}] value={}, timestamp={}", conf->uuid, tv->value, tv->time);
    return tv;
}

void *tester_run(void *runtime)
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

void *tester_configurator(void *configPtr)
{
    return plugin_common::commonConfigurationProcedure<sensor_attribute_t>(
        configPtr, logger, logging, "telemetry-collector::tester", applyFields,
        ::configs);
}


// Global plugin instance
plugin_interface_t plugin =
    {
        get_version,
        tester_create,
        tester_destroy,
        tester_init_sensors,
        tester_finit,
        tester_run,          // run function
        tester_configurator, // configurator function
};