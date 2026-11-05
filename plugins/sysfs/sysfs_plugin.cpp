#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <yaml-cpp/yaml.h>
#include <queue>
#include <algorithm>
#include <map>
#include <unistd.h>
#include <limits.h>
#include <bits/local_lim.h>

#include <string>
#include <fstream>
#include <regex>

#include "sysfs_plugin.hpp"

#include "seanerbus/client/connection.hpp"
#include "seanerbus/client/simple_event_loop.hpp"

#include <seanerbus/helpers/resolve.hpp>

#include <uuid/uuid.h>

static std::map<std::string, SensorReading *> sensor_values;
static Logger logging;
static std::shared_ptr<spdlog::logger> logger;

// Global plugin instance
plugin_interface_t plugin =
    {
        1, // version
        sysfs_create,
        sysfs_destroy,
        sysfs_init_sensors,
        nullptr, // init_actors
        sysfs_finit,
        nullptr,
        nullptr,
        sysfs_run,          // run function
        nullptr,            // req
        nullptr,            // res
        sysfs_configurator, // configurator function
        sysfs_free_config,  // free_config function
};

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

static void setCharField(char *&field, const std::string &value)
{
    delete[] field;
    field = new char[value.size() + 1];
    std::memcpy(field, value.c_str(), value.size() + 1);
}

static std::optional<spdlog::level::level_enum> parse_level(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    auto lvl = spdlog::level::from_str(s);
    if (lvl == spdlog::level::off && s != "off")
        return std::nullopt;
    return lvl;
}

static std::string nameserverPartsToString(const std::map<std::string, std::string> &parts)
{
    std::string result;
    for (const auto &[key, value] : parts)
    {
        result += key + "=" + value + ",";
    }
    if (!result.empty())
    {
        result.pop_back(); // Remove the trailing comma
    }
    return result;
}

void applyFields(const YAML::Node &node, sensor_attribute_t *conf)
{
    if (!node)
        return;

    if (node["uuid"])
    {
        if (node["uuid"].IsScalar() && !node["uuid"].as<std::string>().empty())
        {
            setCharField(conf->uuid, node["uuid"].as<std::string>());
        }
    }
    if (node["topic"])
        setCharField(conf->topic, node["topic"].as<std::string>());
    if (node["interval_ms"])
        conf->interval_ms = std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());
    if (node["description"])
        setCharField(conf->description, node["description"].as<std::string>());
    if (node["unit"])
        setCharField(conf->unit, node["unit"].as<std::string>());
    if (node["path"])
        setCharField(conf->path, node["path"].as<std::string>());
    if (node["filter"])
        setCharField(conf->filter, node["filter"].as<std::string>());
    if (node["factor"])
        conf->factor = node["factor"].as<double>();
    if (node["offset"])
        conf->offset = node["offset"].as<double>();

    if (node["nameserver_parts"])
    {
        for (const auto &p : node["nameserver_parts"])
        {
            std::string k = p.first.as<std::string>();
            ;
            std::string v = p.second.as<std::string>();
            ;
            conf->nameserver_parts[k] = v;
        }
    }
}

bool applyTemplateConfigs(const YAML::Node &node, const YAML::Node &templates, sensor_attribute_t *conf)
{
    if (!node["template"] || !templates)
    {
        return false;
    }

    std::string tmplName = node["template"].as<std::string>();
    YAML::Node tmpl = templates[tmplName];

    if (!tmpl)
    {
        logger->warn("Unknown template referenced: '{}'", tmplName);
        return false;
    }

    applyFields(tmpl, conf);
    return true;
}

bool sysfs_init_sensors(void *runtime)
{
    // Get the plugin config from runtime

    // Connect to the server.
    seanerbus::client::Connection conn("seanerbroker://localhost:5398", "seanerbroker://localhost:5398");

    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));

    size_t responses = 0;
    for (sensor_attribute_t *conf : configs)
    {
        // appending hostname to the name []
        auto it = conf->nameserver_parts.find("hostname");
        const bool has_hostname = it != conf->nameserver_parts.end() && !it->second.empty();

        if (!has_hostname)
        {
            conf->nameserver_parts["hostname"] = hostname;
        }

        // generating name str
        std::string nameserver_parts_str = nameserverPartsToString(conf->nameserver_parts);

        logger->debug("Resolving sensor {}", nameserver_parts_str);
        seanerbus::resolve::resolve(conn, conf->nameserver_parts, [conf, &responses](auto &conn, auto res)
                                    {
            responses++;
            std::string name;
            if (res.ok())
            {
                name = nameserverPartsToString(conf->nameserver_parts);
                std::vector<seanerbus::msg::Entity> entities = res.unwrap_ok().entities();
                if (entities.size() > 0)
                {
                    setCharField(conf->uuid, entities[0].as_string());
                    conf->has_been_resolved = true;
                    logger->debug("Resolved sensor {} successfully to {}", name, conf->uuid);
                } else {
                    logger->debug("Resolved sensor {} successfully but no entities returned", name);
                }
                if (responses == configs.size())
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

    seanerbus::client::Connection conn2("seanerbroker://localhost:5398", "seanerbroker://localhost:5398");
    responses = 0;
    int to_be_registered = 0;
    for (sensor_attribute_t *conf : configs)
    {
        std::string name = nameserverPartsToString(conf->nameserver_parts);

        if (conf->has_been_resolved)
        {
            logger->warn("Sensor {} has been resolved, skipping registration", name);
        }
        else
        {
            if (conf->uuid == nullptr || strlen(conf->uuid) == 0)
            {
                uuid_t id;
                uuid_generate_random(id); // V4

                char uuid[37];
                uuid_unparse_lower(id, uuid);
                setCharField(conf->uuid, std::string(uuid));
                logger->debug("Generated new UUID {}", conf->uuid);
            }

            to_be_registered++;
            seanerbus::msg::Entity entity = seanerbus::msg::Entity{seanerbus::msg::Uuid{conf->uuid}};
            seanerbus::resolve::register_resolve(conn2, conf->nameserver_parts, entity, [conf, &responses, &to_be_registered, name](seanerbus::client::Connection &conn, seanerbus::Expected<void, seanerbus::Error> res)
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
        sensor_t *sensor = new sensor_t();
        // Set the sensor configuration and sense function
        sensor->conf = conf;
        sensor->sense = sensor_sense;

        // Add to the global sensors map using UUID as key
        ::sensors[conf->uuid] = sensor;
        sensor_values[conf->uuid] = new SensorReading{0, 0.0};
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
    for (const sensor_attribute_t *conf : ::configs)
    {
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

uint64_t getTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

double _applyFactorOffset(double value, double factor, double offset)
{
    return (value * factor) + offset;
}

void getPayload(sensor_attribute_t *conf, SensorReading *tv)
{

    FILE *fp = fopen(conf->path, "r");
    if (fp == nullptr)
    {
        logger->error("Error opening file: {} error: {}", conf->path, strerror(errno));
    }

    char buf[1024];
    if (fgets(buf, sizeof(buf), fp) == nullptr)
    {
        fclose(fp);
    }
    fclose(fp);

    // strip trailing newline, if present
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
    {
        buf[len - 1] = '\0';
    }

    std::string valueStr;

    if (!conf->filter)
    {
        valueStr = buf;
    }
    else
    {
        std::cmatch match; // note: cmatch, not smatch, since buf is a char*
        std::regex re(conf->filter);
        if (!std::regex_search(buf, match, re) || match.size() < 2)
        {
            logger->warn("No match for pattern '{}' in line: {}", conf->filter, buf);
        }
        valueStr = match[1].str();
    }

    logger->debug("sensor path {} value {} factor {} offset {}", conf->path, std::stod(valueStr), conf->factor, conf->offset);

    tv->time = getTimestamp();
    tv->value = _applyFactorOffset(std::stod(valueStr), conf->factor, conf->offset);
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

struct ScheduledSensor
{
    std::chrono::steady_clock::time_point next_run;
    sensor_t *sensor;
};
struct CompareNextRun
{
    bool operator()(const ScheduledSensor &a,
                    const ScheduledSensor &b) const
    {
        return a.next_run > b.next_run;
    }
};

void read_publish_schedule_next(seanerbus::client::Connection &conn,
                                std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue);

void on_publish(seanerbus::client::Connection &conn,
                std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue,
                seanerbus::Expected<void, seanerbus::Error> err)
{
    if (!err.ok())
    {
        logger->error("Publishing failed: {}", err.unwrap_error().what());
        conn.shutdown(); // error path only — matches the base example
        return;
    }
    read_publish_schedule_next(conn, queue);
}
void read_publish_schedule_next(seanerbus::client::Connection &conn,
                                std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue)
{
    auto s = queue.top();
    queue.pop();

    auto now = std::chrono::steady_clock::now();
    if (s.next_run > now)
    {
        std::this_thread::sleep_until(s.next_run);
    }

    SensorReading *tv = static_cast<SensorReading *>(s.sensor->sense(s.sensor));

    seanerbus::msg::MessageBuilder<TimeValueV1> msg_builder;
    msg_builder.builder().setTime(tv->time);
    msg_builder.builder().setValue(tv->value);

    seanerbus::msg::Entity entity =
        seanerbus::msg::Entity{seanerbus::msg::Uuid{s.sensor->conf->uuid}};

    s.next_run += s.sensor->conf->interval_ms;
    queue.push(s);

    conn.publish(msg_builder.to_message(entity),
                 [&conn, &queue](seanerbus::client::Connection &c,
                                 seanerbus::Expected<void, seanerbus::Error> e)
                 {
                     on_publish(c, queue, e);
                 });
}
void *sysfs_run(void *runtime)
{

    std::priority_queue<
        ScheduledSensor,
        std::vector<ScheduledSensor>,
        CompareNextRun>
        queue;

    for (auto &s : ::sensors)
    {
        queue.push({std::chrono::steady_clock::now(),
                    s.second});
    }
    seanerbus::client::Connection conn("seanerbroker://localhost:5398", "seanerbroker://localhost:5398");
    read_publish_schedule_next(conn, queue);

    seanerbus::client::SimpleEventLoop sl(conn);
    sl.run();

    if (!conn.result().ok())
    {
        logger->error("publish_test failed: {}", conn.result().get_error().what());
        // return -1;
    }
}

void *sysfs_configurator(void *configPtr)
{
    // This function receives the YAML object from the plugin manager
    // and returns a plugin-specific config object

    // Cast the void* back to YAML::Node*
    YAML::Node *yaml = static_cast<YAML::Node *>(configPtr);
    YAML::Node &config = *yaml;

    const YAML::Node &templates = config["sensor_templates"];
    if (!templates)
    {
        logger->info("No sensor_templates");
    }

    const YAML::Node &log_level = config["log_level"];
    if (log_level)
    {
        auto name = config["log_level"].as<std::string>();
        if (auto level = parse_level(name))
        {
            if (!logger)
                logger = logging.create("telemetry-collector::sysfs");
            logger->set_level(*level);
        }
        else
            logger->error("invalid log_level {} in config", name.c_str());
    }

    if (!config["sensor_groups"])
    {
        logger->error("Missing sensor_groups");
        return yaml;
    }

    // Load sensor groups from the YAML config
    for (const auto &groupEntry : config["sensor_groups"])
    {

        std::string groupName = groupEntry.first.as<std::string>();
        const YAML::Node &group = groupEntry.second;

        // sensor list check
        if (!group["sensors"])
        {
            logger->error("Group '{}' missing sensors list", groupName);
            continue;
        }

        for (const auto &item : group["sensors"])
        {

            // Create a sensor_attribute_t for this sensor
            sensor_attribute_t *conf = new sensor_attribute_t();

            // apply group-level template attr
            if (templates)
            {
                applyTemplateConfigs(group, templates, conf);
            }

            // apply group-level config attr
            applyFields(group, conf);

            // apply sensor-level config attr
            applyFields(item, conf);

            // Add to the global sensors map using UUID as key
            ::configs.push_back(conf);
        }
    }

    return yaml;
}

bool sysfs_free_config(void *config)
{
    if (!config)
    {
        return false;
    }
    return true;
}
