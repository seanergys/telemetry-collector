#pragma once
#include <cstdint>

typedef void* sense_t(void*);
typedef void* act_t(void*);

typedef struct sensor_config sensor_config_t;
typedef struct sensor
{
    sensor_config_t* conf;
    sense_t* sense;
} sensor_t;

typedef struct actor_config actor_config_t;
typedef struct actor
{
    actor_config_t* conf;
    act_t* act;
} actor_t ;