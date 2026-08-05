#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <map>
#include <algorithm>
#include <queue>
#include <yaml-cpp/yaml.h>
#include <string>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include "snmp_plugin.hpp"
#include "../connection.hpp"

static std::map<std::string, seanerbus::msg::TimeValueV1*> sensor_values = {};

// Global plugin instance
plugin_interface_t plugin =
{
    1, // version
    snmp_create,
    snmp_destroy,
    snmp_init_sensors,
    nullptr, // init_actors
    snmp_finit,
    snmp_run,  // run function
    nullptr, // req
    nullptr, // res
    snmp_configurator, // configurator function
    snmp_free_config,   // free_config function
};

void* sensor_sense(void* sensor);


void* snmp_create(void)
{
    // Return a pointer to the plugin_interface_t object as the instance
    return &plugin;
}

bool snmp_destroy(void* runtime)
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


void applyFields(const YAML::Node& node, sensor_attribute_t* conf) {
    if (!node) return;

    if (node["uuid"])           setCharField(conf->uuid,    node["uuid"].as<std::string>());
    if (node["topic"])          setCharField(conf->topic,    node["topic"].as<std::string>());
    if (node["interval_ms"])    conf->interval_ms = std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());
    if (node["description"])    setCharField(conf->description,    node["description"].as<std::string>());
    if (node["unit"])           setCharField(conf->unit,    node["unit"].as<std::string>());
    if (node["filter"])         setCharField(conf->filter, node["filter"].as<std::string>());
    if (node["factor"])         conf->factor     = node["factor"].as<double>();
    if (node["offset"])         conf->offset     = node["offset"].as<double>();
    if (node["connection"])           setCharField(conf->connection,    node["connection"].as<std::string>());
    if (node["community"])  conf->snmpCommunity = node["community"].as<std::string>();
    if (node["oidPrefix"])      conf->oidPrefix = node["oidPrefix"].as<std::string>();
    if (node["username"])       conf->username = node["username"].as<std::string>();
    if (node["authKey"])        conf->authKey = node["authKey"].as<std::string>();
    if (node["privKey"])        conf->privKey = node["privKey"].as<std::string>();
    if (node["authProto"])       conf->authProto = (oid*)node["authProto"].as<oid>();
    if (node["privProto"])       conf->privProto = (oid*)node["privProto"].as<oid>();
    if (node["authProtoLen"])     conf->authProtoLen = node["authProtoLen"].as<size_t>();
    if (node["privProtoLen"])     conf->privProtoLen = node["privProtoLen"].as<size_t>();
    if (node["securityLevel"])     conf->securityLevel = node["securityLevel"].as<int>();
    if (node["version"])     conf->version = node["version"].as<std::string>();

    if (node["type"])     setCharField(conf->type,    node["type"].as<std::string>());
    if (node["oid_prefix"])     conf->oid_prefix,    node["oid_prefix"].as<std::string>();
    if (node["oid_suffix"])     conf->oid_suffix,    node["oid_suffix"].as<std::string>();
}

bool applyTemplateConfigs(const YAML::Node& node, const YAML::Node& templates,sensor_attribute_t* conf) {
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


/*
void setOID(const std::string &oid, sensor_attribute_t* conf) {
    size_t newoidnamelen = MAX_OID_LEN;
    if (read_objid(oid.c_str(), conf->_oid, &newoidnamelen)) {
        conf->_oidLen = newoidnamelen;
    } else {
        conf->_oidLen = 0;
        // LOG(error) << _name << ": Cannot convert OID string: " << oid;
    }
}
*/

bool snmp_init_sensors(void* runtime)
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

bool snmp_finit(void* runtime)
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

void* snmp_run(void* runtime)
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

    return nullptr;
}

void* snmp_configurator(void* configPtr)
{
    // This function receives the YAML object from the plugin manager
    // and returns a plugin-specific config object

    // Cast the void* back to YAML::Node*
    YAML::Node* yaml = static_cast<YAML::Node*>(configPtr);
    YAML::Node& config = *yaml;

    const YAML::Node& templates = config["sensor_templates"];
    if (!templates) {
        std::cout << "No sensor_templates" << std::endl;
    }

    if (!config["sensor_groups"]) {
        std::cout << "Missing sensor_groups" << std::endl;
        return yaml;
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

            // Create a sensor_attribute_t for this sensor
            sensor_attribute_t *conf = new sensor_attribute_t();

            //apply group-level template attr
            if (templates){
                applyTemplateConfigs(group, templates, conf);
            }

            // apply group-level config attr
            applyFields(group, conf);

            //apply sensor-level config attr
            applyFields(item, conf);

            // Add to the global sensors map using UUID as key
            ::configs[conf->uuid] = conf;

        }
    }

    // Since the configurator returns a YAML::Node*, we need to delete it

    return yaml;
}

bool snmp_free_config(void* config)
{
    if (!config)
    {
        return false;
    }
    return true;
}