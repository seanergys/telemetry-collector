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
#include <vector>
#include <string>
#include <fstream>
#include <regex>
#include <cassert>

#include "ad_plugin.hpp"

#include "seanerbus/client/simple_event_loop.hpp"
#include <seanerbus/client/connection.hpp>
#include <seanerbus/helpers/resolve.hpp>

#include <uuid/uuid.h>

static std::map<std::string, SensorReading *> sensor_values = {};
static Logger logging;
static std::shared_ptr<spdlog::logger> logger;

// Global plugin instance
plugin_interface_t plugin =
    {
        1, // version
        ad_create,
        ad_destroy,
        ad_init_sensors,
        nullptr, // init_actors
        ad_finit,
        nullptr,
        nullptr,
        ad_run,          // run function
        nullptr,         // req
        nullptr,         // res
        ad_configurator, // configurator function
        ad_free_config,  // free_config function
};

void *sensor_sense(void *sensor);

// Global sensor values map for the sense function

void *ad_create(void)
{
    // Return a pointer to the plugin_interface_t object as the instance
    logger = logging.create("telemetry-collector::ad");
    return &plugin;
}

bool ad_destroy(void *runtime)
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

    if (node["topic"])
        setCharField(conf->topic, node["topic"].as<std::string>());
    if (node["interval_ms"])
        conf->interval_ms = std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());
    if (node["description"])
        setCharField(conf->description, node["description"].as<std::string>());
    if (node["unit"])
        setCharField(conf->unit, node["unit"].as<std::string>());
    //if (node["connection_uuid"])
    //    setCharField(conf->connection_uuid, node["connection_uuid"].as<std::string>());
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

bool ad_init_sensors(void *runtime)
{
    for (const auto &[uuid, conf] : configs)
    {
        sensor_t *sensor = new sensor_t();

        // Set the sensor configuration and sense function
        sensor->conf = conf;
        sensor->sense = sensor_sense;

        // Add to the global sensors map using UUID as key
        ::sensors[uuid] = sensor;
        sensor_values[conf->uuid] = new SensorReading{0, 0.0};

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

void req_handler(seanerbus::client::Connection &conn, seanerbus::Expected<seanerbus::msg::Message, seanerbus::Error> res)
{

    if (!res.ok())
    {
        auto err = res.unwrap_error();
        logger->error("Failed to listen for requests: {}", err.what());
        conn.shutdown(seanerbus::Unexpected(err));
        return;
    }

    auto msg = std::move(res.unwrap_ok());

    if (msg.payload_type() != HelloWorldResV1::TYPE)
    {
        logger->error("Got unknown return message type!");
        return;
    }

    auto rd_res = seanerbus::msg::MessageReader<HelloWorldResV1>::from_msg(msg);

    if (!rd_res.ok())
    {
        logger->error("Failed to decode message as HelloWorldReqV1: {}", rd_res.unwrap_error().what());
        conn.shutdown();
        return;
    }

    auto rd = std::move(rd_res.unwrap_ok());

    logger->info("Got answer to request: {}", rd.reader().getMsg().cStr());

    conn.shutdown();
}

void getPayload(sensor_attribute_t *conf, SensorReading *tv)
{
    // todo: replace
    auto entity = seanerbus::msg::Entity{seanerbus::msg::Uuid{conf->uuid}};
    // Connect to the server.
    seanerbus::client::Connection conn("demobus://localhost:5398", "demobus://localhost:5398");

    seanerbus::msg::MessageBuilder<HelloWorldReqV1> bd;

    bd.builder().setMsg("Hello World");

    conn.request(bd.to_message(entity), req_handler);

    seanerbus::client::SimpleEventLoop sel(conn);

    sel.run();

    if (!conn.result().ok())
    {
        logger->error("./request failed: {}", conn.result().get_error().what());
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

void *ad_run(void *runtime)
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
    }

    return nullptr;
}

void *ad_configurator(void *configPtr)
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

        // this is not needed, since we get the list through interaction with the naming service
        // sensor list check
        /*
            if (!group["sensors"])
            {
                logger->error("Group '{}' missing sensors list", groupName);
                continue;
            }
        */

        if (!group["nameserver_parts"])
        {
            logger->error("Group '{}' missing name key value list", groupName);
            continue;
        }

        std::map<std::string, std::string> name;
        for (const auto &item : group["nameserver_parts"])
        {
            // todo: parse the name from group["nameserver_parts"]
            name[item.first.as<std::string>()] = item.second.as<std::string>();
        }

        for (const auto &item : name)
        {

            logger->debug("Name mapping: {} = {}", item.first, item.second);
        }
        seanerbus::client::Connection conn("demobus://localhost:5398", "demobus://localhost:5398");

        std::vector<seanerbus::msg::Entity> entities;
        seanerbus::resolve::resolve(
            conn,
            name,
            [&entities](auto &conn, auto res)
            {
                if (!res.ok())
                {
                    logger->error("Resolve not ok: {}", res.unwrap_error().what());
                    conn.shutdown();
                    return;
                }
                entities = res.unwrap_ok().entities();
                for (const auto &item : entities)
                {
                    logger->debug("Resolved entity: {}", item.as_string());
                }
                conn.shutdown();
            });
        seanerbus::client::SimpleEventLoop sel(conn);
        sel.run();

        for (const auto &item : entities)
        {

            // todo create a sensor object
            // todo maybe reverse query the naming service to look up the fields and possibly set them if needed

            // Create a sensor_attribute_t for this sensor
            sensor_attribute_t *conf = new sensor_attribute_t();

            conf->uuid = strdup(item.as_string().c_str());

            logger->info("Creating sensor with UUID: {}", conf->uuid);
            // apply group-level template attr
            if (templates)
            {
                applyTemplateConfigs(group, templates, conf);
            }

            // apply group-level config attr
            applyFields(group, conf);

            // apply sensor-level config attr
            // todo: maybe? we may be able to get reverse resolve the names and set the fields?
            // applyFields(item, conf);

            // Add to the global sensors map using UUID as key
            ::configs[conf->uuid] = conf;
        }
    }
    logger->info("Total sensors configured: {}", ::configs.size());
    assert(::configs.size() != 0);

    return yaml;
}

bool ad_free_config(void *config)
{
    if (!config)
    {
        return false;
    }
    return true;
}
