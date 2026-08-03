#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../plugin.hpp"
#include "../types.hpp"

/* lifecycle */
void* tester_create(void);
bool  tester_destroy(void* runtime);
bool  tester_init_sensors(void* runtime);
bool  tester_init_actors(void* runtime);
bool  tester_finit(void* runtime);
void* tester_run(void* runtime);

/* config loading */
void* tester_configurator(void* config);
bool  tester_free_config(void* config);

typedef struct sensor_config
{
    char* uuid;
    char* topic;
    std::chrono::milliseconds interval_ms;
    char* unit;
} sensor_config_t;

std::map<std::string, sensor_config_t*> configs; // uuid -> configs
std::map<std::string, sensor_t*> sensors; // uuid -> sensors

/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;