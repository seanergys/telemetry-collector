// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include <atomic>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <regex>
#include <stdexcept>

#include "ad_plugin.hpp"
#include "../plugin_common.hpp"

#include "seanerbus/client/simple_event_loop.hpp"
#include <seanerbus/client/connection.hpp>
#include <seanerbus/helpers/resolve.hpp>

static std::map<std::string, SensorReading *> sensor_values = {};
static Logger logging;
static std::shared_ptr<spdlog::logger> logger;

version_t* get_version (void){
    return nullptr;
}

void *sensor_sense(void *sensor);

// Global sensor values map for the sense function

void *ad_create(void)
{
    // Return a pointer to the plugin_interface_t object as the instance
    if (!logger)
        logger = logging.create("telemetry-collector::ad");
    return &plugin;
}

bool ad_destroy(void *runtime)
{
    // The plugin_interface_t object is statically allocated, so we don't free it
    // Just clean up any resources if needed
    false;
}

void applyFields(const YAML::Node &node, sensor_attribute_t *conf)
{
    plugin_common::applyCommonSensorFields(
        node, conf, plugin_common::UuidPolicy::GenerateWhenInvalid);
}

bool ad_init_sensors(void *runtime)
{
    for (const auto &[uuid, conf] : configs)
    {
        plugin_common::initializeSensor(
            uuid, conf, sensor_sense, ::sensors, sensor_values);
    }
    return true; // We're using the global map, not returning a pointer to sensors
}

bool ad_finit(void *runtime)
{
    for (auto &pair : ::configs)
    {
        sensor_attribute_t *conf = pair.second;
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

void req_handler(seanerbus::client::Connection &conn, seanerbus::Expected<seanerbus::msg::Message, seanerbus::Error> res, SensorReading *tv)
{

    if (!res.ok())
    {
        auto err = res.unwrap_error();
        logger->error("Failed to listen for requests: {}", err.what());
        conn.shutdown(seanerbus::Unexpected(err));
        return;
    }

    auto msg = std::move(res.unwrap_ok());



    if (msg.payload_type() != GetResV1::TYPE)
    {
        logger->error("Got unknown return message type!");
        return;
    }

    // TODO uncomment this and remove the code below until before the connection shutdown.
    // This is now using a new message patch in seanerbus so we can only test this after
    // we upgreade the seanerbus instance
    /*
    auto rd_res = seanerbus::msg::MessageReader<GetReqV1>::from_msg(msg);

    if (!rd_res.ok())
    {
        std::cerr << "Could not parse message as :GetReqV1 " << rd_res.unwrap_error().what();
        return;
    }
    auto rd = rd_res.unwrap_ok();

    //logger->info("Got answer 1 to request: value={}, time={}", rd.reader().getValue(), rd.reader().getTime());

    tv->value = rd.reader().getValue();
    tv->time = rd.reader().getTime();
    logger->info("Got answer 2 to request: value={}, time={}", tv->value, tv->time);
    */

    auto payload = msg.payload();

    if (payload.size() != sizeof(uint64_t) * 2)
    {
        logger->error("Invalid payload size: {}", payload.size());
        conn.shutdown();
        return;
    }

    uint64_t values[2];
    std::memcpy(values, payload.data(), sizeof(values));

    logger->info("Got answer: value={}, time={}",
                 values[0], values[1]);

    tv->value = values[0];
    tv->time = values[1];
    logger->info("Got answer 2 to request: value={}, time={}", tv->value, tv->time);

    conn.shutdown();
}

void getPayload(sensor_attribute_t *conf, SensorReading *tv)
{
    auto entity = seanerbus::msg::Entity{seanerbus::msg::Uuid{conf->uuid}};
    // Connect to the server.
    auto conn = plugin_common::makeConnection(conf->broker_url);

    seanerbus::msg::MessageBuilder<GetReqV1> bd;

    conn.request(
        bd.to_message(entity),
        [tv](seanerbus::client::Connection &conn, auto res)
        {
            req_handler(conn, std::move(res), tv);
    	    logger->info("Got answer, right after the request: value={}, time={}", tv->value, tv->time);
        }
    );

    seanerbus::client::SimpleEventLoop sel(conn);

    sel.run();

    if (!conn.result().ok())
    {
        logger->error("request failed: {}", conn.result().get_error().what());
    }
}

// Sense function with correct signature
void *sensor_sense(void *sensor)
{
    sensor_t *s = (sensor_t *)sensor;
    sensor_attribute_t *conf = s->conf;

    SensorReading *tv = sensor_values[conf->uuid];

    getPayload(conf, tv);
    return tv;
}

void *ad_run(void *runtime)
{
    return plugin_common::runSensors(
        ::sensors,
        [](sensor_t *sensor)
        {
            const auto *reading =
                static_cast<SensorReading *>(sensor->sense(sensor));
            logger->info("Publishing: value={}, time={}", reading->value, reading->time);
            return plugin_common::makeTimeValueMessage(
                sensor, reading->time, reading->value);
        },
        runtime,
        logger);
}

void *ad_configurator(void *configPtr)
{
    // This function receives the YAML object from the plugin manager
    // and returns a plugin-specific config object

    if (!logger)
        logger = logging.create("telemetry-collector::ad");

    // Cast the void* back to YAML::Node*
    YAML::Node *yaml = static_cast<YAML::Node *>(configPtr);
    YAML::Node &config = *yaml;
    const auto brokerUrl = plugin_common::seanerbusUrl(config);

    const YAML::Node &templates = config["sensor_templates"];
    if (!templates)
    {
        logger->info("No sensor_templates");
    }
    plugin_common::applyLogLevel(
        config, logger, logging, "telemetry-collector::ad");

    if (!plugin_common::parseGroups(
            config,
            [&](const std::string &groupName, const YAML::Node &group)
            {
                // This is not needed; we get the list through the naming service.
                if (!group["nameserver_parts"])
                {
                    logger->error("Group '{}' missing name key value list", groupName);
                    return;
                }

                std::map<std::string, std::string> name;
                for (const auto &part : group["nameserver_parts"])
                {
                    auto key = part.first.as<std::string>();
                    auto value = part.second.as<std::string>();
                    name[key] = value;
                }

                logger->debug(
                    "Resolving AD sensor group '{}' with name-server parts: {}",
                    groupName,
                    plugin_common::nameserverPartsToString(name));

                auto conn = [&]
                {
                    try
                    {
                        return plugin_common::makeConnection(brokerUrl);
                    }
                    catch (const std::exception &error)
                    {
                        throw std::runtime_error(
                            "Could not connect to SeanerBUS while resolving AD "
                            "sensor group '" +
                            groupName + "': " + error.what());
                    }
                }();
                std::vector<seanerbus::msg::Entity> entities;
                plugin_common::resolveName(
                    conn,
                    name,
                    [&entities, groupName](
                        auto &connection, auto result)
                    {
                        if (!result.ok())
                        {
                            logger->error(
                                "Name-server resolution failed for AD sensor group '{}': {}",
                                groupName,
                                result.unwrap_error().what());
                            connection.shutdown();
                            return;
                        }

                        auto resolvedEntities = result.unwrap_ok().entities();
                        logger->debug(
                            "Name-server resolution for AD sensor group '{}' returned {} entities",
                            groupName,
                            resolvedEntities.size());
                        for (const auto &entity : resolvedEntities)
                        {
                            logger->debug(
                                "Resolved AD sensor entity '{}' in group '{}'",
                                entity.as_string(),
                                groupName);
                            entities.push_back(entity);
                        }

                        connection.shutdown();
                    });
                seanerbus::client::SimpleEventLoop sel(conn);
                sel.run();

                if (entities.empty())
                {
                    logger->warn(
                        "No AD sensor entities resolved for group '{}'",
                        groupName);
                }

                for (const auto &entity : entities)
                {
                    // Create a sensor for each resolved entity.
                    auto *conf = new sensor_attribute_t();
                    conf->uuid = strdup(entity.as_string().c_str());
                    conf->broker_url = brokerUrl;
                    plugin_common::applyTemplateConfig(
                        group, templates, conf, applyFields, logger);
                    applyFields(group, conf);
                    ::configs[conf->uuid] = conf;
                }
            },
            logger))
        return nullptr;

    if (::configs.empty())
    {
        logger->error("AD configuration produced no sensors");
        return nullptr;
    }

    logger->debug("Configured {} AD sensors", ::configs.size());

    return yaml;
}

// Global plugin instance
plugin_interface_t plugin =
    {
        get_version,
        ad_create,
        ad_destroy,
        ad_init_sensors,
        ad_finit,
        ad_run,          // run function
        ad_configurator, // configurator function
};