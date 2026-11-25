// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include "mock_plugin.hpp"
#include <yaml-cpp/yaml.h>
#include <thread>
#include <atomic>

static std::atomic<bool> running(false);

version_t* get_version (void){
    return nullptr;
}

void* mock_create(void)
{
    return &plugin;
}

bool mock_destroy(void* runtime)
{
    // The plugin_interface_t object is statically allocated, so we don't free it
    // Just clean up any resources if needed
    return true;
}

bool mock_init_sensors(void* runtime)
{
    // Allocate sensors and return them
    // The plugin manager will store these in runtime->sensors
    // todo

    return true;
}

bool mock_finit(void* runtime)
{
    // Free sensors and actors from runtime->sensors and runtime->actors
    // todo

    return true;
}

void* mock_run(void* runtime)
{
    // The run function contains the main execution loop
    // It should check the running flag and execute sensors/actors accordingly
    (void)runtime; // Suppress unused parameter warning

    // Simple implementation: just set running flag and wait
    // In a real plugin, this would contain the actual execution logic
    while (running.load())
    {
        // Execute sensors and actors here
        // This is where the while loop from the old design would go
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return nullptr;
}

void* mock_configurator(void* configPtr)
{
    YAML::Node *yaml = static_cast<YAML::Node *>(configPtr);
    YAML::Node &config = *yaml;

    return yaml;
}

extern "C" plugin_interface_t plugin =
{
    get_version,
    mock_create,
    mock_destroy,
    mock_init_sensors,  // init_sensors function
    mock_finit,
    mock_run,           // run function
    mock_configurator,   // configurator function
};