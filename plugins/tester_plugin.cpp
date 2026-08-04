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
#include "../connection.hpp"

static std::map<std::string, seanerbus::msg::TimeValueV1*> sensor_values = {};

// Global plugin instance
plugin_interface_t plugin =
{
    1, // version
    tester_create,
    tester_destroy,
    tester_init_sensors,
    nullptr, // init_actors
    tester_finit,
    tester_run,  // run function
    nullptr, // req
    nullptr, // res
    tester_configurator, // configurator function
    tester_free_config,   // free_config function
};

void* sensor_sense(void* sensor);


void* tester_create(void)
{
    // Return a pointer to the plugin_interface_t object as the instance
    return &plugin;
}

bool tester_destroy(void* runtime)
{
    // The plugin_interface_t object is statically allocated, so we don't free it
    // Just clean up any resources if needed
    false;
}

bool tester_init_sensors(void* runtime)
{
    // Get the plugin config from runtime

    for (const auto& [uuid, conf] : configs){
        sensor_t *sensor = new sensor_t();

        // Set the sensor configuration and sense function
        sensor->conf = conf;
        sensor->sense = sensor_sense;

        // Add to the global sensors map using UUID as key
        ::sensors[uuid] = sensor;
        sensor_values[conf->uuid] = new seanerbus::msg::TimeValueV1(0,0.0);
    }

    return true; // We're using the global map, not returning a pointer to sensors
}

bool tester_finit(void* runtime)
{

    // Clear the map
    for (auto& pair : ::configs)
    {
        sensor_attribute_t* conf = pair.second;
        if (conf)
        {
            delete conf->uuid;
            delete conf->topic;
            delete conf->unit;
            delete conf;
        }
    }

    // Clean up the global sensors map
    for (auto& pair : ::sensors)
    {
        sensor_attribute_t* conf = pair.second->conf;
        if (conf)
        {
            delete conf->uuid;
            delete conf->topic;
            delete conf->unit;
            delete conf;
        }

        sensor_t* sensor = pair.second;
        if (sensor)
            delete sensor;

    }
    ::sensors.clear();
    ::configs.clear();


    for (auto& pair : sensor_values)
    {
        if (pair.second)
            delete pair.second;
    }
    // Clear the sensor_values map as well
    sensor_values.clear();

    // Reset num_sensors in runtime
    //if (runtime) {
    //    ((runtime_t*)runtime)->num_sensors = 0;
    //}

    return true;
}

// Sense function with correct signature
void* sensor_sense(void* sensor)
{
    sensor_t* s = (sensor_t*)sensor;
    sensor_attribute_t* conf = s->conf;

    seanerbus::msg::TimeValueV1 *tv = sensor_values[conf->uuid];

    // Print the sensor data
    std::cout << "[" << conf->uuid << "] value=" << tv->value++
              << ", timestamp=" << tv->time++ << std::endl;
    return tv;
}

struct ScheduledSensor {
    std::chrono::steady_clock::time_point next_run;
    sensor_t* sensor;
};
struct CompareNextRun
{
    bool operator()(const ScheduledSensor& a,
                    const ScheduledSensor& b) const
    {
        return a.next_run > b.next_run;
    }
};

void* tester_run(void* runtime)
{

    std::priority_queue<
        ScheduledSensor,
        std::vector<ScheduledSensor>,
        CompareNextRun> queue;

    for (auto& s : ::sensors)
    {
        queue.push({
            std::chrono::steady_clock::now(),
            s.second
        });
    }

    while (((runtime_t*)runtime)->running)
    {
        auto now = std::chrono::steady_clock::now();

        // Sleep until the next sensor is due.
        if (queue.top().next_run > now)
            std::this_thread::sleep_until(queue.top().next_run);

        now = std::chrono::steady_clock::now();


        // Process every sensor that's ready.
        while (!queue.empty() && queue.top().next_run <= now)
        {
            auto s = queue.top();
            queue.pop();

            seanerbus::msg::TimeValueV1 *tv = (seanerbus::msg::TimeValueV1*) s.sensor->sense(s.sensor);
            //seanerbus::msg::TimeValueV1 *tv = (seanerbus::msg::TimeValueV1*) sensor_sense(s.sensor);

            test_publish(tv, s.sensor->conf->uuid);

            s.next_run += s.sensor->conf->interval_ms;
            queue.push(s);
        }
    }

    /*

    while (((runtime_t*)runtime)->running)
    {
        for (auto& sensor : ::sensors)
        {
            //{
            //    seanerbus::msg::TimeValueV1 tv = *(seanerbus::msg::TimeValueV1*) sensor.second->sense(sensor.second);
            //    test_publish(tv, sensor.second->conf->uuid);
            //}

            {
                seanerbus::msg::TimeValueV1 *tv = (seanerbus::msg::TimeValueV1*) sensor_sense(sensor.second);
                test_publish(*tv, sensor.second->conf->uuid);
            }
        }

        auto it = sensors.begin();
        if (it != sensors.end()) {
            // note this is not doing the right thing, we better imlement the groups first to avoid
            // the  commplex scheduling logic here
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));//it->second.conf->interval_ms));
        } else {
            // If no sensors, sleep for a default interval
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

    }
    */

    return nullptr;
}

void* tester_configurator(void* configPtr)
{
    // This function receives the YAML object from the plugin manager
    // and returns a plugin-specific config object

    // Cast the void* back to YAML::Node*
    YAML::Node* yaml = static_cast<YAML::Node*>(configPtr);

    // Clear any existing sensors in the map

    // Load sensors from the YAML config -> move to configurator
    if ((*yaml)["sensors"].IsSequence())
    {
    for (const auto& node : (*yaml)["sensors"])
    {
            // Create a sensor_attribute_t for this sensor
            sensor_attribute_t* conf = new sensor_attribute_t();

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

bool tester_free_config(void* config)
{
    if (!config)
    {
        return false;
    }
    return true;
}