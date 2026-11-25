// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

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
bool ad_finit(void *runtime);
void *ad_run(void *runtime);

/* config loading */
void *ad_configurator(void *config);

typedef struct sensor_attribute : generic_sensor_attribute_t
{
    char *connection_uuid;
    std::string broker_url;

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
