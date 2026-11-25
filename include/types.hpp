// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once
#include <cstdint>
#include <chrono>
#include <map>
#include <string>

typedef void* sense_t(void*);
typedef void* act_t(void*);

struct generic_sensor_attribute_t
{
    char *uuid = nullptr;
    char *topic = nullptr;
    std::chrono::milliseconds interval_ms{0};
    char *description = nullptr;
    char *unit = nullptr;
    char *filter = nullptr;
    double factor = 1;
    double offset = 0;
    std::map<std::string, std::string> nameserver_parts;
    bool uuid_specified_in_config = false;
};

typedef struct sensor_attribute sensor_attribute_t;
typedef struct sensor
{
    sensor_attribute_t* conf;
    sense_t* sense;
} sensor_t;

typedef struct actor_config actor_config_t;
typedef struct actor
{
    actor_config_t* conf;
    act_t* act;
} actor_t ;