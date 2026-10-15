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

#include "tester_plugin.hpp"

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
        tester_create,
        tester_destroy,
        tester_init_sensors,
        nullptr, // init_actors
        tester_finit,
        nullptr,
        nullptr,
        tester_run,          // run function
        nullptr,             // req
        nullptr,             // res
        tester_configurator, // configurator function
        tester_free_config,  // free_config function
};

void *sensor_sense(void *sensor);

// Global sensor values map for the sense function

void *tester_create(void)
{
    logger = logging.create("telemetry-collector::tester");
    // Return a pointer to the plugin_interface_t object as the instance
    return &plugin;
}

bool tester_destroy(void *runtime)
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
        else
        {
            uuid_t id;
            uuid_generate_random(id); // V4

            char buf[37];
            uuid_unparse_lower(id, buf);
            std::string uuid(buf);

            setCharField(conf->uuid, uuid);
        }
    }

    if (node["interval_ms"])
        conf->interval_ms = std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());
    if (node["description"])
        setCharField(conf->description, node["description"].as<std::string>());
    if (node["unit"])
        setCharField(conf->unit, node["unit"].as<std::string>());

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

bool tester_init_sensors(void *runtime)
{
    // Get the plugin config from runtime

    // Connect to the server.
    seanerbus::client::Connection conn("demobus://localhost:5398", "demobus://localhost:5398");

    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));

    for (const auto &[uuid, conf] : configs)
    {
        sensor_t *sensor = new sensor_t();

        // Set the sensor configuration and sense function
        sensor->conf = conf;
        sensor->sense = sensor_sense;

        // Add to the global sensors map using UUID as key
        ::sensors[uuid] = sensor;
        sensor_values[conf->uuid] = new SensorReading{0, 0.0};

        auto entity = seanerbus::msg::Entity{seanerbus::msg::Uuid{conf->uuid}};

        // appending hostname to the name []
        auto it = conf->nameserver_parts.find("hostname");
        const bool has_hostname = it != conf->nameserver_parts.end() && !it->second.empty();

        if (!has_hostname)
        {
            conf->nameserver_parts["hostname"] = hostname;
        }

        std::string nameserver_parts_str;
        for (const auto &[k, v] : conf->nameserver_parts)
        {
            nameserver_parts_str += k + "=" + v + ",";
        }
        nameserver_parts_str.pop_back();
        logger->info("Registering sensor {{}}: {}", nameserver_parts_str, conf->uuid);

        seanerbus::resolve::register_resolve(conn, conf->nameserver_parts, entity,
                                             [](seanerbus::client::Connection &conn,
                                                seanerbus::Expected<void, seanerbus::Error> res)
                                             {
                                                 if (!res.ok())
                                                 {
                                                     conn.shutdown();
                                                     return;
                                                 }
                                                 conn.shutdown();
                                             });
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

uint64_t getTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// Sense function with correct signature
void *sensor_sense(void *sensor)
{
    sensor_t *s = (sensor_t *)sensor;
    sensor_attribute_t *conf = s->conf;

    SensorReading *tv = sensor_values[conf->uuid];
    tv->time = getTimestamp();
    tv->value++;

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

void *tester_run(void *runtime)
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
    seanerbus::client::Connection conn("demobus://localhost:5398", "demobus://localhost:5398");
    read_publish_schedule_next(conn, queue);

    seanerbus::client::SimpleEventLoop sl(conn);
    sl.run();

    if (!conn.result().ok())
    {
        logger->error("publish_test failed: {}", conn.result().get_error().what());
        // return -1;
    }
}

void *tester_configurator(void *configPtr)
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
            logger->info("Group {} missing sensors list", groupName);
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
            ::configs[conf->uuid] = conf;
        }
    }
    return yaml;
}

bool tester_free_config(void *config)
{
    if (!config)
    {
        return false;
    }
    return true;
}
