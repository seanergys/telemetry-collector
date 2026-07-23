#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include <string>
#include <fstream>

#include "sysfs_plugin.hpp"
#include "../connection.hpp"

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

// Global sensor values map for the sense function
static std::unordered_map<std::string, double> sensor_values;

// Global running flag for the run function
static std::atomic<bool> running(true);

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
    YAML::Node *yaml = static_cast<YAML::Node *>(((runtime_t *)runtime)->config);

    // Clear any existing sensors in the map

    // Load sensors from the YAML config -> move to configurator
    if ((*yaml)["sensors"].IsSequence())
    {
        for (const auto &node : (*yaml)["sensors"])
        {
            sensor_t *sensor = new sensor_t();

            // Create a sensor_config_t for this sensor
            sensor_config_t *conf = new sensor_config_t();

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

            // Copy path
            
            std::string path_str = node["path"].as<std::string>();
            conf->path = new char[path_str.size() + 1];
            std::memcpy(conf->path, path_str.c_str(), path_str.size() + 1);
            
            // Set interval
            conf->interval_ms = std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());

            // Set the sensor configuration and sense function
            sensor->conf = conf;
            sensor->sense = sensor_sense;

            // Add to the global sensors map using UUID as key
            ::sensors[conf->uuid] = sensor;
        }
    }

    // Store the number of sensors in runtime for reference

    return true; // We're using the global map, not returning a pointer to sensors
}

bool tester_finit(void *runtime)
{
    // Clean up the global sensors map
    for (auto &pair : ::sensors)
    {
        sensor_config_t *conf = pair.second->conf;
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

    // Clear the map
    ::sensors.clear();

    // Clear the sensor_values map as well
    sensor_values.clear();

    // Reset num_sensors in runtime
    // if (runtime) {
    //    ((runtime_t*)runtime)->num_sensors = 0;
    //}

    return true;
}

// Sense function with correct signature
void *sensor_sense(void *sensor)
{
    sensor_t *s = (sensor_t *)sensor;
    sensor_config_t *conf = s->conf;

    // Generate a value (incrementing counter)
    double value = ++sensor_values[conf->uuid];

    // Print the sensor data
    std::cout << "[" << conf->uuid << "] test value=" << value
              << ", .... timestamp=" << std::time(nullptr) << std::endl;
    return nullptr;
}

uint64_t getTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void getPayload(std::string path, seanerbus::msg::TimeValueV1 *tv)
{
    std::cout << "path" << path << std::endl;
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cout << "Error opening  file from :" << path << " error:" << strerror(errno) << std::endl;
    }

    double value = 0;
    file >> value;

    if (file.fail())
    {
        std::cout << "Error reading  file from :" << path << " error:" << strerror(errno) << std::endl;
    }

    std::cout << "sensor value" << value << std::endl;

    tv->time = getTimestamp();
    tv->value = value;
    
    
}

void *tester_run(void *runtime)
{

    seanerbus::msg::TimeValueV1 tv(0, 0.0);

    while (((runtime_t *)runtime)->running)
    {
        for (auto &sensor : ::sensors)
        {
            sensor_config_t *conf = sensor.second->conf;
            
            getPayload(conf->path, &tv);

            test_publish(tv, conf->uuid);
           
        }

        auto it = sensors.begin();
        if (it != sensors.end())
        {
            // note this is not doing the right thing, we better imlement the groups first to avoid
            // the  commplex scheduling logic here
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // it->second.conf->interval_ms));
        }
        else
        {
            // If no sensors, sleep for a default interval
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    return nullptr;
}

void *tester_configurator(void *configPtr)
{
    // This function receives the YAML object from the plugin manager
    // and returns a plugin-specific config object

    // Cast the void* back to YAML::Node*
    YAML::Node *yaml = static_cast<YAML::Node *>(configPtr);
    // Create a plugin-specific config object using tester_config_t

    /*
    tester_config_t* config = new tester_config_t();

    // Load the config values from the YAML object
    config->name = new char[(*yaml)["name"].as<std::string>().size() + 1];
    std::memcpy(
        config->name,
        (*yaml)["name"].as<std::string>().c_str(),
        (*yaml)["name"].as<std::string>().size() + 1);

    config->uuid = new char[(*yaml)["uuid"].as<std::string>().size() + 1];
    std::memcpy(
        config->uuid,
        (*yaml)["uuid"].as<std::string>().c_str(),
        (*yaml)["uuid"].as<std::string>().size() + 1);

    for (const auto& node : (*yaml)["sensors"])
    {
        sensor_config_t sensor{};

        sensor.uuid = new char[node["uuid"].as<std::string>().size() + 1];
        std::memcpy(
            sensor.uuid,
            node["uuid"].as<std::string>().c_str(),
            node["uuid"].as<std::string>().size() + 1);

        sensor.topic = new char[node["topic"].as<std::string>().size() + 1];
        std::memcpy(
            sensor.topic,
            node["topic"].as<std::string>().c_str(),
            node["topic"].as<std::string>().size() + 1);

        sensor.interval_ms = node["interval_ms"].as<uint64_t>();

        sensor.unit = new char[node["unit"].as<std::string>().size() + 1];
        std::memcpy(
            sensor.unit,
            node["unit"].as<std::string>().c_str(),
            node["unit"].as<std::string>().size() + 1);

        config->sensors.push_back(sensor);
    }
    */

    return yaml;
}

bool tester_free_config(void *config)
{
    if (!config)
    {
        return false;
    }

    // Since the configurator returns a YAML::Node*, we need to delete it
    YAML::Node *yaml = static_cast<YAML::Node *>(config);
    delete yaml;

    return true;
}
