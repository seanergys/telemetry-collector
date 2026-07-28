#include "mock_plugin.hpp"
#include <thread>
#include <atomic>

static std::atomic<bool> running(false);

void* mock_create(void)
{
    return &plugin;
}

bool mock_destroy(void* runtime)
{
    // The plugin_interface_t object is statically allocated, so we don't free it
    // Just clean up any resources if needed
    return false;
}

bool mock_init_sensors(void* runtime)
{
    // Allocate sensors and return them
    // The plugin manager will store these in runtime->sensors
    // todo

    return false;
}
bool mock_init_actors(void* runtime)
{
    // Allocate actors and return them
    // The plugin manager will store these in runtime->actors
    // todo

    return false;
}
bool mock_finit(void* runtime)
{
    // Free sensors and actors from runtime->sensors and runtime->actors
    // todo

    return false;
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

void* mock_configurator(void* config)
{
    // This plugin doesn't need to parse YAML config
    // It just returns nullptr as the config
    (void)config; // Suppress unused parameter warning
    return nullptr;
}

bool mock_free_config(void* config)
{
    // Nothing to free since we return nullptr
    // todo
    return false;
}

extern "C" plugin_interface_t plugin =
{
    1, // version
    mock_create,
    mock_destroy,
    mock_init_sensors,  // init_sensors function
    mock_init_actors,   // init_actors function
    mock_finit,
    mock_run,           // run function
    nullptr, // req
    nullptr, // res
    mock_configurator,   // configurator function
    mock_free_config,   // free_config function
};

