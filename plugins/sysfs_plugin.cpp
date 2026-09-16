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

#include <string>
#include <fstream>
#include <regex>

#include "sysfs_plugin.hpp"

#include <seanerbus/client/connection.hpp>
#include "seanerbus/client/simple_event_loop.hpp"

#include <seanerbus/helpers/resolve.hpp>

static std::map<std::string, SensorReading *> sensor_values;

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

void applyFields(const YAML::Node &node, sensor_attribute_t *conf)
{
    if (!node)
        return;

    if (node["uuid"])
        setCharField(conf->uuid, node["uuid"].as<std::string>());
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

    if (node["name_part"])
    {
	for (const auto & p : node["name_part"]) {
    	    std::string k = p.first.as<std::string>();;
    	    std::string v =p.second.as<std::string>();;
            conf->name[k] = v;
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
        std::cout << "Unknown template referenced: '" << tmplName << "'" << std::endl;
        return false;
    }

    applyFields(tmpl, conf);
    return true;
}

bool sysfs_init_sensors(void *runtime)
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

        auto addr = seanerbus::msg::Address{seanerbus::msg::Uuid{conf->uuid}};

        conf->name["hostname"] = std::string(hostname);

        seanerbus::resolve::register_resolve(conn, conf->name, addr,
                                             [](seanerbus::client::Connection &conn,
                                                seanerbus::Expected<void, seanerbus::Error> res)
                                             {
                                                 if (!res.ok())
                                                 {
                                                     conn.shutdown();
                                                     return true;
                                                 }
                                             });
    }
    
    seanerbus::client::SimpleEventLoop sl(conn);
    sl.run();

    conn.shutdown();
    return true; // We're using the global map, not returning a pointer to sensors
}


bool sysfs_finit(void *runtime)
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

void getPayload(sensor_attribute_t *conf, SensorReading *tv)
{

    FILE *fp = fopen(conf->path, "r");
    if (fp == nullptr)
    {
        std::cout << "Error opening file: " << conf->path
                  << " error: " << strerror(errno) << std::endl;
    }

    char buf[1024];
    if (fgets(buf, sizeof(buf), fp) == nullptr)
    {
        std::cout << "Error reading line from: " << conf->path << std::endl;
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
            std::cout << "No match for pattern '" << conf->filter << "' in line: " << buf << std::endl;
        }
        valueStr = match[1].str();
    }

    std::cout << "sensor path" << conf->path << " value " << std::stod(valueStr) << " factor" << conf->factor << " offset" << conf->offset << std::endl;

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
    std::cout << "[" << conf->uuid << "] value=" << tv->value << ", timestamp=" << tv->time << std::endl;
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
        std::cerr << "Publishing failed: " << err.unwrap_error().what() << "\n";
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

    seanerbus::msg::Address addr =
        seanerbus::msg::Address{seanerbus::msg::Uuid{s.sensor->conf->uuid}};

    s.next_run += s.sensor->conf->interval_ms;
    queue.push(s);

    conn.publish(msg_builder.to_message(addr),
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
    seanerbus::client::Connection conn("demobus://localhost:5398", "demobus://localhost:5398");
    read_publish_schedule_next(conn, queue);

    seanerbus::client::SimpleEventLoop sl(conn);
    sl.run();

    if (!conn.result().ok())
    {
        std::cerr << "publish_test failed: " << conn.result().get_error().what();
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
        std::cout << "No sensor_templates" << std::endl;
    }

    if (!config["sensor_groups"])
    {
        std::cout << "Missing sensor_groups" << std::endl;
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
            std::cout << "Group '" << groupName << "' missing sensors list" << std::endl;
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

bool sysfs_free_config(void *config)
{
    if (!config)
    {
        return false;
    }
    return true;
}
