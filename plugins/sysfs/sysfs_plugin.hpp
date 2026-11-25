// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "logger.hpp"
#include "plugin.hpp"
#include "types.hpp"
#include <optional>

/* lifecycle */
void *sysfs_create(void);
bool sysfs_destroy(void *runtime);
bool sysfs_init_sensors(void *runtime);
bool sysfs_finit(void *runtime);
void *sysfs_run(void *runtime);

/* config loading */
void *sysfs_configurator(void *config);

typedef struct sensor_attribute : generic_sensor_attribute_t
{
    bool has_been_resolved = false;
    char *hostname;
    char *path;
} sensor_attribute_t;

struct SensorReading
{
    uint64_t time = 0;
    double value = 0.0;
};

std::map<std::string, sensor_attribute_t *> configs;
std::map<std::string, sensor_t *> sensors; // uuid -> sensors

/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;
