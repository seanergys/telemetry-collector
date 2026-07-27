#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../plugin.hpp"
#include "../sensor.hpp"

/* lifecycle */
void* sysfs_create(void);
bool  sysfs_destroy(void* runtime);
bool  sysfs_init_sensors(void* runtime);
bool  sysfs_init_actors(void* runtime);
bool  sysfs_finit(void* runtime);
void* sysfs_run(void* runtime);

/* config loading */
void* sysfs_configurator(void* config);
bool  sysfs_free_config(void* config);

typedef struct sensor_config
{
    char* uuid;
    char* topic;
    std::chrono::milliseconds interval_ms;
    char* unit;
    char* path;
    char* filter;
    double factor;
    double offset;
} sensor_config_t;

std::map<std::string, sensor_t*> sensors; // uuid -> sensors

/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;