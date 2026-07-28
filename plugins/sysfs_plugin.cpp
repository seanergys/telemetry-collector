#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include <string>
#include <fstream>
#include <regex>


#include "sysfs_plugin.hpp"
#include "../connection.hpp"

// Global plugin instance
plugin_interface_t plugin =
    {
        1, // version
        sysfs_create,
        sysfs_destroy,
        sysfs_init_sensors,
        nullptr, // init_actors
        sysfs_finit,
        sysfs_run,          // run function
        nullptr,             // req
        nullptr,             // res
        sysfs_configurator, // configurator function
        sysfs_free_config,  // free_config function
};

void *sensor_sense(void *sensor);

// Global sensor values map for the sense function
static std::unordered_map<std::string, double> sensor_values;

// Global running flag for the run function
static std::atomic<bool> running(true);

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


static void setCharField(char*& field, const std::string& value) {
    delete[] field;             
    field = new char[value.size() + 1];
    std::memcpy(field, value.c_str(), value.size() + 1);
}

void applyFields(const YAML::Node& node, sensor_config_t* conf) {
    if (!node) return;

    if (node["uuid"])           setCharField(conf->uuid,    node["uuid"].as<std::string>());
    if (node["topic"])          setCharField(conf->topic,    node["topic"].as<std::string>());
    if (node["interval_ms"])    conf->interval_ms = std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());
    if (node["description"])    setCharField(conf->description,    node["description"].as<std::string>());
    if (node["unit"])           setCharField(conf->unit,    node["unit"].as<std::string>());
    if (node["path"])           setCharField(conf->path,    node["path"].as<std::string>());
    if (node["filter"])         setCharField(conf->filter, node["filter"].as<std::string>());
    if (node["factor"])         conf->factor     = node["factor"].as<double>();
    if (node["offset"])         conf->offset     = node["offset"].as<double>();

   
}

bool applyTemplateConfigs(const YAML::Node& node, const YAML::Node& templates,sensor_config_t* conf) {
    if (!node["template"] || !templates) {
        return false;
    }

    std::string tmplName = node["template"].as<std::string>();
    YAML::Node tmpl = templates[tmplName];

    if (!tmpl) {
        std::cout << "Unknown template referenced: '" << tmplName << "'" << std::endl ;
        return false;
    }

    applyFields(tmpl, conf);
    return true;
}

bool sysfs_init_sensors(void *runtime)
{

    // Get the plugin config from runtime
    YAML::Node *yaml = static_cast<YAML::Node *>(((runtime_t *)runtime)->config);
    YAML::Node& config = *yaml;

    const YAML::Node& templates = config["sensor_templates"];
    if (!templates) {
        std::cout << "No sensor_templates" << std::endl;
    }

    if (!config["sensor_groups"]) {
        std::cout << "Missing sensor_groups" << std::endl;
        return false;
    }

    // Load sensor groups from the YAML config
    for (const auto& groupEntry : config["sensor_groups"]) {
        
        std::string groupName = groupEntry.first.as<std::string>();
        const YAML::Node& group = groupEntry.second;

        // sensor list check
        if (!group["sensors"]) {
            std::cout << "Group '" << groupName << "' missing sensors list" << std::endl;
            continue;
        }

        for (const auto& item : group["sensors"]){
            sensor_t *sensor = new sensor_t();

            // Create a sensor_config_t for this sensor
            sensor_config_t *conf = new sensor_config_t();
            
            //apply group-level template attr
            if (templates){
                applyTemplateConfigs(group, templates, conf);
            }
            
            // apply group-level config attr
            applyFields(group, conf);

            //apply sensor-level config attr
            applyFields(item, conf);
            
            // Set the sensor configuration and sense function
            sensor->conf = conf;
            sensor->sense = sensor_sense;

            // Add to the global sensors map using UUID as key
            ::sensors[conf->uuid] = sensor;

            
        }
    }
   
    return true; // We're using the global map, not returning a pointer to sensors

}

bool sysfs_finit(void *runtime)
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


double _applyFactorOffset(double value, double factor, double offset) {
    return (value * factor) + offset;
}

void getPayload(sensor_config_t* conf,seanerbus::msg::TimeValueV1 *tv)
{

    FILE* fp = fopen(conf->path, "r");
    if (fp == nullptr) {
        std::cout << "Error opening file: " << conf->path
                   << " error: " << strerror(errno) << std::endl;
    }

    char buf[1024];
    if (fgets(buf, sizeof(buf), fp) == nullptr) {
        std::cout << "Error reading line from: " << conf->path << std::endl;
        fclose(fp);
    }
    fclose(fp);

    // strip trailing newline, if present
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }


    std::string valueStr;

    if (!conf->filter) {
        valueStr = buf;
    } else {
        std::cmatch match;   // note: cmatch, not smatch, since buf is a char*
        std::regex re(conf->filter);
        if (!std::regex_search(buf, match, re) || match.size() < 2) {
            std::cout << "No match for pattern '" << conf->filter << "' in line: " << buf << std::endl;
        }
        valueStr = match[1].str();
    }

    //std::cout << "sensor path" << conf->path << " value " << std::stod(valueStr) <<" factor" << conf->factor << " offset"<< conf->offset << std::endl;
    
    tv->time = getTimestamp();
    tv->value = _applyFactorOffset(std::stod(valueStr), conf->factor, conf->offset);
    
}

void *sysfs_run(void *runtime)
{

    seanerbus::msg::TimeValueV1 tv(0, 0.0);

    while (((runtime_t *)runtime)->running)
    {
        for (auto &sensor : ::sensors)
        {
            sensor_config_t *conf = sensor.second->conf;

            getPayload(conf, &tv);
                         
            test_publish(tv, conf->uuid);
            //std::cout.flush();
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

void *sysfs_configurator(void *configPtr)
{
    // This function receives the YAML object from the plugin manager
    // and returns a plugin-specific config object

    // Cast the void* back to YAML::Node*
    YAML::Node *yaml = static_cast<YAML::Node *>(configPtr);
    // Create a plugin-specific config object using sysfs_config_t

    /*
    sysfs_config_t* config = new sysfs_config_t();

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

bool sysfs_free_config(void *config)
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
