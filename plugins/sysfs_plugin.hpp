#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../plugin.hpp"
#include "../types.hpp"

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

typedef struct sensor_attribute
{
    char* uuid;
    char* topic;
    std::chrono::milliseconds interval_ms;
    char* description;
    char* unit;
    char* path;
    char* filter;
    double factor = 1;
    double offset = 0;
} sensor_attribute_t;

std::map<std::string, sensor_attribute_t*> configs; // uuid -> configs
std::map<std::string, sensor_t*> sensors; // uuid -> sensors


/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;