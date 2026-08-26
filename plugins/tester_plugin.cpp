#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <map>
#include <algorithm>
#include <queue>
#include <yaml-cpp/yaml.h>

#include "tester_plugin.hpp"

#include <seanerbus/msg/time_value_v1.hpp>
#include <seanerbus/client/pub_sub/connection.hpp>
#include "seanerbus/client/simple_event_loop.hpp"
#include "seanerbus/helpers/noexcept.hpp"

static std::map<std::string, seanerbus::msg::TimeValueV1 *> sensor_values = {};

// Global plugin instance
plugin_interface_t plugin =
    {
        1, // version
        tester_create,
        tester_destroy,
        tester_init_sensors,
        nullptr, // init_actors
        tester_finit,
        tester_run,          // run function
        nullptr,             // req
        nullptr,             // res
        tester_configurator, // configurator function
        tester_free_config,  // free_config function
};

void *sensor_sense(void *sensor);

void *tester_create(void)
{
    // Return a pointer to the plugin_interface_t object as the instance
    return &plugin;
}

bool tester_destroy(void *runtime)
{
    // The plugin_interface_t object is statically allocated, so we don't free it
    // Just clean up any resources if needed
    false;
}

bool tester_init_sensors(void *runtime)
{
    // Get the plugin config from runtime

    for (const auto &[uuid, conf] : configs)
    {
        sensor_t *sensor = new sensor_t();

        // Set the sensor configuration and sense function
        sensor->conf = conf;
        sensor->sense = sensor_sense;

        // Add to the global sensors map using UUID as key
        ::sensors[uuid] = sensor;
        sensor_values[conf->uuid] = new seanerbus::msg::TimeValueV1(0, 0.0);
    }

    return true; // We're using the global map, not returning a pointer to sensors
}

bool tester_finit(void *runtime)
{

    // Clear the map
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

    // Reset num_sensors in runtime
    // if (runtime) {
    //    ((runtime_t*)runtime)->num_sensors = 0;
    //}

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

    seanerbus::msg::TimeValueV1 *tv = sensor_values[conf->uuid];

    tv->time = getTimestamp();

    // Print the sensor data
    // std::cout << "[" << conf->uuid << "] value=" << tv->value<< ", timestamp=" << tv->time << std::endl;
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

void schedule_next(seanerbus::client::pub_sub::Connection &conn,
                   std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue);

void on_publish_done(seanerbus::client::pub_sub::Connection &conn,
                     std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue,
                     seanerbus::Expected<void, seanerbus::Error> err)
{
    if (!err.ok())
    {
        std::cerr << "Publishing failed: " << err.unwrap_error().what() << "\n";
        conn.shutdown(); // error path only — matches the base example
        return;
    }
    schedule_next(conn, queue);
}

void schedule_next(seanerbus::client::pub_sub::Connection &conn,
                   std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue)
{
    auto s = queue.top();
    queue.pop();

    auto now = std::chrono::steady_clock::now();
    if (s.next_run > now)
    {
        std::this_thread::sleep_until(s.next_run);
    }

    seanerbus::msg::TimeValueV1 *tv = (seanerbus::msg::TimeValueV1 *)s.sensor->sense(s.sensor);

    seanerbus::msg::MessageBuilder<TimeValueV1> msg_builder;
    msg_builder.builder().setTime(tv->time);
    msg_builder.builder().setValue(tv->value);

    seanerbus::msg::Address addr =
        seanerbus::msg::Address{seanerbus::msg::Uuid{s.sensor->conf->uuid}};

    s.next_run += s.sensor->conf->interval_ms;
    queue.push(s);

    conn.publish(msg_builder.to_message(addr),
                 [&conn, &queue](seanerbus::client::pub_sub::Connection &c,
                                 seanerbus::Expected<void, seanerbus::Error> e)
                 {
                     on_publish_done(c, queue, e);
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

    seanerbus::client::pub_sub::Connection conn("demobus://localhost:5398");
    schedule_next(conn, queue);

    seanerbus::client::SimpleEventLoop sl;
    sl.register_pub_sub_connection(conn);
    sl.run();

    if (!conn.result().ok())
    {
        std::cerr << "publish_test failed: " << conn.result().get_error().what();
        // return -1;
    }
}

void *tester_configurator(void *configPtr)
{
    // This function receives the YAML object from the plugin manager
    // and returns a plugin-specific config object

    // Cast the void* back to YAML::Node*
    YAML::Node *yaml = static_cast<YAML::Node *>(configPtr);

    // Clear any existing sensors in the map

    // Load sensors from the YAML config -> move to configurator
    if ((*yaml)["sensors"].IsSequence())
    {
        for (const auto &node : (*yaml)["sensors"])
        {
            // Create a sensor_attribute_t for this sensor
            sensor_attribute_t *conf = new sensor_attribute_t();

            // Copy UUID
            std::string uuid_str = node["uuid"].as<std::string>();
            conf->uuid = new char[uuid_str.size() + 1];
            std::memcpy(conf->uuid, uuid_str.c_str(), uuid_str.size() + 1);

            // Copy topic
            std::string topic_str = node["topic"].as<std::string>();
            conf->topic = new char[topic_str.size() + 1];
            std::memcpy(conf->topic, topic_str.c_str(), topic_str.size() + 1);
            // Copy unit
            std::string unit_str = node["unit"].as<std::string>();
            conf->unit = new char[unit_str.size() + 1];
            std::memcpy(conf->unit, unit_str.c_str(), unit_str.size() + 1);

            // Set interval
            conf->interval_ms = std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());

            // Add to the global sensors map using UUID as key
            ::configs[conf->uuid] = conf;
        }
    }

    // Since the configurator returns a YAML::Node*, we need to delete it

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