// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "plugin.hpp"
#include "logger.hpp"
#include <seanerbus/client/connection.hpp>
#include <seanerbus/client/simple_event_loop.hpp>
#include <seanerbus/helpers/resolve.hpp>
#include <spdlog/spdlog.h>
#include <uuid/uuid.h>
#include <yaml-cpp/yaml.h>

#include "types.hpp"

namespace plugin_common
{

inline std::string seanerbusUrl(const YAML::Node &config)
{
    const YAML::Node urlNode = config["seanerbus"]["url"];
    if (!urlNode || !urlNode.IsScalar())
        throw std::invalid_argument("Missing or invalid seanerbus.url");

    const auto url = urlNode.as<std::string>();
    if (url.empty())
        throw std::invalid_argument("seanerbus.url must not be empty");

    return url;
}

inline seanerbus::client::Connection makeConnection(const std::string &url)
{
    if (url.empty())
        throw std::invalid_argument("seanerbus.url must not be empty");

    return seanerbus::client::Connection(url, url);
}

inline seanerbus::client::Connection makeConnection(void *runtime)
{
    if (!runtime)
        throw std::invalid_argument("Plugin runtime is null");

    const auto *runtimeState = static_cast<const runtime_t *>(runtime);
    return makeConnection(runtimeState->seanerbus_url);
}

inline void setCharField(char *&field, const std::string &value)
{
    delete[] field;
    field = new char[value.size() + 1];
    std::memcpy(field, value.c_str(), value.size() + 1);
}

inline std::optional<spdlog::level::level_enum> parseLogLevel(std::string level)
{
    std::transform(level.begin(), level.end(), level.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    const auto parsed = spdlog::level::from_str(level);
    if (parsed == spdlog::level::off && level != "off")
        return std::nullopt;
    return parsed;
}

inline void applyLogLevel(
    const YAML::Node &config,
    Logger::LoggerPtr &logger,
    Logger &logging,
    const std::string &loggerName)
{
    const YAML::Node logLevel = config["log_level"];
    if (!logLevel)
        return;

    const auto name = logLevel.as<std::string>();
    if (auto level = parseLogLevel(name))
    {
        if (!logger)
            logger = logging.create(loggerName);
        logger->set_level(*level);
    }
    else
    {
        logger->error("invalid log_level {} in config", name.c_str());
    }
}

inline uint64_t getTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

inline double applyFactorOffset(double value, double factor, double offset)
{
    return (value * factor) + offset;
}

template <typename Logger>
std::string extractFilteredValue(
    const char *value,
    const char *filter,
    const Logger &logger)
{
    if (!filter)
        return value;

    std::cmatch match;
    const std::regex expression(filter);
    if (!std::regex_search(value, match, expression) || match.size() < 2)
        logger->warn("No match for pattern '{}' in line: {}", filter, value);

    return match[1].str();
}

inline std::string generateUuid()
{
    uuid_t id;
    uuid_generate_random(id);
    char value[37];
    uuid_unparse_lower(id, value);
    return value;
}

inline bool isValidUuid(const std::string &value)
{
    uuid_t parsed;
    return uuid_parse(value.c_str(), parsed) == 0;
}

inline bool applyResolvedUuid(
    generic_sensor_attribute_t &config,
    const std::string &resolvedUuid)
{
    if (config.uuid_specified_in_config)
        return config.uuid && resolvedUuid == config.uuid;

    setCharField(config.uuid, resolvedUuid);
    return true;
}

inline void addHostname(
    std::map<std::string, std::string> &nameserverParts,
    const char *hostname)
{
    const auto it = nameserverParts.find("hostname");
    if (it == nameserverParts.end() || it->second.empty())
        nameserverParts["hostname"] = hostname;
}

inline std::string nameserverPartsToString(
    const std::map<std::string, std::string> &nameserverParts)
{
    std::string result;
    for (const auto &[key, value] : nameserverParts)
        result += key + "=" + value + ",";

    if (!result.empty())
        result.pop_back();
    return result;
}

template <typename SensorConfig, typename SensorMap, typename ReadingMap>
void initializeSensor(
    const std::string &key,
    SensorConfig *config,
    sense_t *sense,
    SensorMap &sensors,
    ReadingMap &readings)
{
    auto *sensor = new sensor_t();
    sensor->conf = config;
    sensor->sense = sense;
    sensors[key] = sensor;

    using Reading = std::remove_pointer_t<typename ReadingMap::mapped_type>;
    readings[config->uuid] = new Reading{0, 0.0};
}

struct ScheduledSensor
{
    std::chrono::steady_clock::time_point next_run;
    sensor_t *sensor;
};

struct CompareNextRun
{
    bool operator()(const ScheduledSensor &a, const ScheduledSensor &b) const
    {
        return a.next_run > b.next_run;
    }
};

using SensorSchedule =
    std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun>;

inline auto makeTimeValueMessage(sensor_t *sensor, uint64_t time, double value)
{
    seanerbus::msg::MessageBuilder<TimeValueV1> messageBuilder;
    messageBuilder.builder().setTime(time);
    messageBuilder.builder().setValue(value);

    const auto entity =
        seanerbus::msg::Entity{seanerbus::msg::Uuid{sensor->conf->uuid}};
    return messageBuilder.to_message(entity);
}

template <typename MessageFactory, typename Logger>
void scheduleNextPublish(
    seanerbus::client::Connection &connection,
    SensorSchedule &schedule,
    MessageFactory messageFactory,
    Logger logger)
{
    auto scheduled = schedule.top();
    schedule.pop();

    if (scheduled.next_run > std::chrono::steady_clock::now())
        std::this_thread::sleep_until(scheduled.next_run);

    auto message = messageFactory(scheduled.sensor);
    scheduled.next_run += scheduled.sensor->conf->interval_ms;
    schedule.push(scheduled);

    connection.publish(
        message,
        [&schedule, messageFactory, logger](
            seanerbus::client::Connection &conn,
            seanerbus::Expected<void, seanerbus::Error> result)
        {
            if (!result.ok())
            {
                logger->error("Publishing failed: {}", result.unwrap_error().what());
                conn.shutdown();
                return;
            }

            scheduleNextPublish(conn, schedule, messageFactory, logger);
        });
}

template <typename SensorMap, typename MessageFactory, typename Logger>
void *runSensors(
    SensorMap &sensors,
    MessageFactory messageFactory,
    void *runtime,
    Logger logger)
{
    SensorSchedule schedule;
    for (auto &entry : sensors)
        schedule.push({std::chrono::steady_clock::now(), entry.second});

    if (schedule.empty())
        return nullptr;

    auto connection = makeConnection(runtime);
    scheduleNextPublish(connection, schedule, messageFactory, logger);

    seanerbus::client::SimpleEventLoop eventLoop(connection);
    eventLoop.run();

    if (!connection.result().ok())
        logger->error("publish_test failed: {}", connection.result().get_error().what());

    return nullptr;
}

template <typename Callback>
void resolveName(
    seanerbus::client::Connection &connection,
    const std::map<std::string, std::string> &nameserverParts,
    Callback &&callback)
{
    seanerbus::resolve::resolve(
        connection, nameserverParts, std::forward<Callback>(callback));
}

template <typename Callback>
void registerSensor(
    seanerbus::client::Connection &connection,
    const std::map<std::string, std::string> &nameserverParts,
    const char *uuid,
    Callback &&callback)
{
    const auto entity = seanerbus::msg::Entity{seanerbus::msg::Uuid{uuid}};
    seanerbus::resolve::register_resolve(
        connection, nameserverParts, entity, std::forward<Callback>(callback));
}

inline void registerSensor(
    seanerbus::client::Connection &connection,
    const std::map<std::string, std::string> &nameserverParts,
    const char *uuid)
{
    registerSensor(
        connection, nameserverParts, uuid,
        [](seanerbus::client::Connection &conn,
           seanerbus::Expected<void, seanerbus::Error> result)
        {
            if (!result.ok())
            {
                conn.shutdown();
                return;
            }
            conn.shutdown();
        });
}

enum class UuidPolicy
{
    GenerateWhenInvalid,
    IgnoreInvalid
};

inline void applyUuid(
    const YAML::Node &node,
    generic_sensor_attribute_t &config,
    UuidPolicy policy)
{
    if (!node["uuid"])
        return;

    if (node["uuid"].IsScalar())
    {
        const auto value = node["uuid"].as<std::string>();
        if (isValidUuid(value))
        {
            setCharField(config.uuid, value);
            config.uuid_specified_in_config = true;
            return;
        }
    }

    if (policy == UuidPolicy::GenerateWhenInvalid)
        setCharField(config.uuid, generateUuid());
}

struct CommonSensorFieldOptions
{
    bool factor_offset = true;
    bool topic_filter = true;
};

void applyCommonSensorFields(
    const YAML::Node &node,
    generic_sensor_attribute_t *config,
    UuidPolicy uuidPolicy,
    CommonSensorFieldOptions options = {})
{
    if (!node)
        return;

    applyUuid(node, *config, uuidPolicy);
    if (node["interval_ms"])
        config->interval_ms =
            std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());
    if (node["description"])
        setCharField(config->description, node["description"].as<std::string>());
    if (node["unit"])
        setCharField(config->unit, node["unit"].as<std::string>());
    if (options.factor_offset && node["factor"])
        config->factor = node["factor"].as<double>();
    if (options.factor_offset && node["offset"])
        config->offset = node["offset"].as<double>();

    if (node["nameserver_parts"])
    {
        for (const auto &part : node["nameserver_parts"])
        {
            config->nameserver_parts[part.first.as<std::string>()] =
                part.second.as<std::string>();
        }
    }
    if (options.topic_filter && node["topic"])
        setCharField(config->topic, node["topic"].as<std::string>());
    if (options.topic_filter && node["filter"])
        setCharField(config->filter, node["filter"].as<std::string>());
}

template <typename SensorConfig, typename ApplyFields, typename Logger>
bool applyTemplateConfig(
    const YAML::Node &node,
    const YAML::Node &templates,
    SensorConfig *config,
    ApplyFields applyFields,
    const Logger &logger)
{
    if (!node["template"] || !templates)
        return false;

    const auto name = node["template"].as<std::string>();
    const YAML::Node templateNode = templates[name];
    if (!templateNode)
    {
        logger->warn("Unknown template referenced: '{}'", name);
        return false;
    }

    applyFields(templateNode, config);
    return true;
}

template <typename GroupCallback, typename Logger>
bool parseGroups(
    const YAML::Node &config,
    GroupCallback parseGroup,
    const Logger &logger)
{
    const YAML::Node groups = config["sensor_groups"];
    if (!groups)
    {
        logger->error("Missing sensor_groups");
        return false;
    }

    for (const auto &groupEntry : groups)
    {
        const auto groupName = groupEntry.first.as<std::string>();
        const YAML::Node &group = groupEntry.second;
        parseGroup(groupName, group);
    }

    return true;
}

template <typename MissingSensorList, typename SensorCallback>
void parseSensors(
    const YAML::Node &group,
    const std::string &groupName,
    MissingSensorList missingSensorList,
    SensorCallback parseSensor)
{
    if (!group["sensors"])
    {
        missingSensorList(groupName);
        return;
    }

    for (const auto &sensor : group["sensors"])
        parseSensor(sensor);
}

template <typename SensorConfig, typename ApplyFields>
void *commonConfigurationProcedure(
    void *configPtr,
    Logger::LoggerPtr &logger,
    ::Logger &logging,
    const std::string &loggerName,
    ApplyFields applyFields,
    std::map<std::string, SensorConfig *> &configs)
{
    static_assert(
        std::is_base_of_v<generic_sensor_attribute_t, SensorConfig>,
        "Sensor configs must derive from generic_sensor_attribute_t");

    auto *yaml = static_cast<YAML::Node *>(configPtr);
    const YAML::Node &config = *yaml;

    const YAML::Node &templates = config["sensor_templates"];
    if (!templates)
        logger->info("No sensor_templates");

    applyLogLevel(config, logger, logging, loggerName);

    parseGroups(
        config,
        [&](const std::string &groupName, const YAML::Node &group)
        {
            parseSensors(
                group, groupName,
                [&](const std::string &name)
                {
                    logger->error("Group '{}' missing sensors list", name);
                },
                [&](const YAML::Node &sensor)
                {
                    auto sensorConfig = std::make_unique<SensorConfig>();
                    applyTemplateConfig(
                        group, templates, sensorConfig.get(), applyFields, logger);
                    applyFields(group, sensorConfig.get());
                    applyTemplateConfig(
                        sensor, templates, sensorConfig.get(), applyFields, logger);
                    applyFields(sensor, sensorConfig.get());
                    if (!sensorConfig->uuid || sensorConfig->uuid[0] == '\0')
                        setCharField(sensorConfig->uuid, generateUuid());
                    auto &storedConfig = configs[sensorConfig->uuid];
                    delete storedConfig;
                    storedConfig = sensorConfig.release();
                });
        },
        logger);

    return yaml;
}

} // namespace plugin_common