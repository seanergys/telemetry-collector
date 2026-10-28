#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "logger.hpp"
#include "plugin.hpp"
#include "types.hpp"
#include <optional>

/* lifecycle */
void *ad_create(void);
bool ad_destroy(void *runtime);
bool ad_init_sensors(void *runtime);
bool ad_init_actors(void *runtime);
bool ad_finit(void *runtime);
void *ad_run(void *runtime);

/* config loading */
void *ad_configurator(void *config);
bool ad_free_config(void *config);
static std::optional<spdlog::level::level_enum> parse_level(std::string s);

typedef struct sensor_attribute
{
    char *uuid;
    char *topic;
    std::chrono::milliseconds interval_ms;
    char *description;
    char *unit;
    char *connection_uuid;
    char *filter;
    double factor = 1;
    double offset = 0;
    std::map<std::string, std::string> nameserver_parts;

} sensor_attribute_t;

struct SensorReading
{
    uint64_t time = 0;
    double value = 0.0;
};

std::map<std::string, sensor_attribute_t *> configs; // uuid -> configs
std::map<std::string, sensor_t *> sensors;           // uuid -> sensors

/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;
