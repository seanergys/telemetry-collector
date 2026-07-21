#pragma once
#include <cstdint>

typedef void* sense_t(void*);
typedef void* act_t(void*);

typedef struct sensor_config
{
    char* uuid;
    char* topic;
    uint64_t interval_ms;
    char* unit;

} sensor_config_t;

typedef struct sensor
{
    sensor_config_t* conf;

    sense_t* sense;
} sensor_t;

typedef struct actor
{
    char* uuid;
    char* topic;
    uint64_t interval_ms;
    char* unit;

    act_t* act;
} actor_t ;