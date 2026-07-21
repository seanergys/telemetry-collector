#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../plugin.hpp"
#include "../sensor.hpp"

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

std::map<std::string, sensor_t> sensors; // uuid -> sensors

/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;