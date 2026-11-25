// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

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

#include "types.hpp"
#include "semver.h"

typedef struct pluginConfig
{
    char* path;
    char* uuid;
    char* config;
    char* name;
    std::string seanerbus_url;
} config_t;

typedef version_t* get_version_t(void);
typedef void* create_t(void);
typedef bool  destroy_t(void*);
typedef bool  init_sensors_t (void*);
typedef bool  finit_t (void*);
typedef bool  init_connections_t (void*);
typedef bool  finit_connections_t (void*);

typedef void* configurator_t (void*);

typedef void* run_t(void*); // typedef void* publish_t(void*);

//typedef seanerbus::client::Connection* connection_t;
typedef void* connection_t;

// This contains only the function pointers and version info - the static interface
typedef struct
{
    get_version_t* get_version;

    create_t*  create;
    destroy_t* destroy;

    init_sensors_t* init_sensors;
    finit_t*    finit;

    run_t* run;

    configurator_t* configurator;  // Plugin-specific configurator function

} plugin_interface_t;

// Runtime state (managed by plugin manager)
// This struct contains the runtime resources and state
typedef struct
{
    // Library
    void* library;

    // Plugin descriptor
    plugin_interface_t* plugin;

    // Using boolean flags instead of pointers for status checking
    bool sensors_initialized;
    connection_t* connection;
    void* config;  // Plugin-specific configuration (YAML or parsed data)
    std::string seanerbus_url;

    bool loaded;
    bool initialized;
    bool running;
} runtime_t;