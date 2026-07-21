#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <stdbool.h>

#include "sensor.hpp"

#include <seanerbus/client/connection.hpp>
#include <seanerbus/msg/time_value_v1.hpp>

struct Payload
{
    std::uint64_t timestamp;
    double value;
};

class SeanerbusWrapper
{
public:
    void publish(std::string uuid, Payload payload)
    {
        std::cout << "[" << uuid << "]: timestamp=" << payload.timestamp << ", value=" << payload.value << '\n';

        std::string broker = "localhost";
        std::string port = "5398";

        seanerbus::client::Connection conn(broker.c_str(), port.c_str());

        auto addr = seanerbus::Address{seanerbus::Uuid{uuid}};

        seanerbus::msg::TimeValueV1 tv{
            payload.timestamp,
            payload.value
        };

        conn.publish(addr, tv.to_capnp());
    }
};

typedef struct pluginConfig
{
    char* path;
    char* uuid;
    char* config;
    char* name;
} config_t;

typedef void* create_t(void);
typedef bool  destroy_t(void*);
typedef bool  init_sensors_t (void*);
typedef bool  init_actors_t (void*);
typedef bool  finit_t (void*);

typedef void* configurator_t (void*);
typedef bool  free_config_t(void*);

typedef void* run_t(void*); // typedef void* publish_t(void*);
typedef void* req_t(void*);
typedef void* res_t(void*);

//typedef seanerbus::client::Connection* connection_t;
typedef void* connection_t;

// This contains only the function pointers and version info - the static interface
typedef struct
{
    uint32_t version;

    create_t*  create;
    destroy_t* destroy;

    init_sensors_t* init_sensors;
    init_actors_t* init_actors;
    finit_t*    finit;

    run_t* run; // publish
    req_t* req; // method to make requests (e.g., to the access daemon)
    res_t* res; // method to make requests (e.g., to the access daemon);

    configurator_t* configurator;  // Plugin-specific configurator function
    free_config_t* free_config;

    // Note: sensors, actors, connection, config moved to runtime_t (runtime state)
} plugin_interface_t;

// Runtime state (managed by plugin manager)
// This contains the runtime resources and state
typedef struct
{
    void* library;

    plugin_interface_t* plugin;  // Plugin descriptor (function pointers)

    // Runtime state (moved from plugin_interface_t)
    // Using boolean flags instead of pointers for status checking
    bool sensors_initialized;
    bool actors_initialized;
    connection_t* connection;
    void* config;  // Plugin-specific configuration (YAML or parsed data)

    bool loaded;
    bool initialized;
    bool running;
} runtime_t;

