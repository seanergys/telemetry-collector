#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "plugin.hpp"
#include "types.hpp"

/* lifecycle */
void* ad_create(void);
bool  ad_destroy(void* runtime);
bool  ad_init_sensors(void* runtime);
bool  ad_init_actors(void* runtime);
bool  ad_finit(void* runtime);
void* ad_run(void* runtime);

/* config loading */
void* ad_configurator(void* config);
bool  ad_free_config(void* config);
typedef struct sensor_attribute
{
    char* uuid;
    char* topic;
    std::chrono::milliseconds interval_ms;
    char* description;
    char* unit;
    char* connection_uuid;
    char* filter;
    double factor = 1;
    double offset = 0;
} sensor_attribute_t;

struct SensorReading
{
    uint64_t time = 0;
    double value = 0.0;
};

std::map<std::string, sensor_attribute_t*> configs; // uuid -> configs
std::map<std::string, sensor_t*> sensors; // uuid -> sensors
std::shared_ptr<spdlog::logger> logger; /* logger */

/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;

