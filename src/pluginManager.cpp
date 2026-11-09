// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include "pluginManager.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <yaml-cpp/yaml.h>

#include <dlfcn.h>

#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

namespace
{
    constexpr uint32_t PLUGIN_VERSION = 1;
} // namespace

bool PluginManager::is_plugin_loaded(
    const char *uuid)
{
    return find_runtime(uuid) != nullptr;
}

bool PluginManager::finit_plugin(
    const char *uuid)
{
    auto *runtime =
        find_runtime(uuid);

    if (!runtime)
    {
        return false;
    }

    // Check if plugin is running - cannot finalize a running plugin
    if (runtime->running)
    {
        logger->error("Plugin \"{}\" is still running. Stop the plugin before finalizing.", uuid);
    return false;
}

    // Check if plugin is initialized before finalizing
    if (!runtime->initialized)
    {
        logger->error("Plugin \"{}\" is not initialized", uuid);
        return false;
    }

    // Free the config using plugin's free_config function
    if (runtime->plugin->free_config && runtime->config)
    {
        runtime->plugin->free_config(runtime->config);
        runtime->config = nullptr;
    }

    if (runtime->plugin->finit)
    {
        runtime->plugin->finit(runtime);
    }

    runtime->sensors_initialized = false;
    runtime->actors_initialized = false;
    runtime->initialized = false;
    return true;
}


runtime_t *PluginManager::find_runtime(
    const char *uuid)
{
    auto it = runtimes.find(uuid);
    if (it == runtimes.end())
    {
        return nullptr;
    }
    return &it->second;
}

PluginManager::~PluginManager()
{
    while (!runtimes.empty())
    {
        auto it = runtimes.begin();
        // Stop the plugin if it's running before unloading
        if (it->second.running)
        {
            stop_plugin(it->first.c_str());
        }

        if (it->second.initialized)
        {
            finit_plugin(it->first.c_str());
        }

        if (it->second.loaded){
            unload_plugin(it->first.c_str());
        }

    }
    //runtimes.clear();
    //threads.clear();
}

bool PluginManager::load_plugin(
    const char *path,
    const char *configFile,
    const char *uuid)
{
    // Check if plugin with this UUID is already loaded
    if (is_plugin_loaded(uuid))
    {
        logger->error("Plugin with UUID \"{}\" is already loaded", uuid);
    return false;
}

    // Create the entry in the class member map first and work with it directly
    auto& runtime = runtimes[uuid];

    // Load the library
    runtime.library = dlopen(path, RTLD_NOW);
    if (!runtime.library)
    {
        logger->error("dlopen failed: {}", dlerror());
        runtimes.erase(uuid);
        return false;
    }

    // Get the plugin descriptor
    runtime.plugin = reinterpret_cast<plugin_interface_t*>(dlsym(runtime.library, "plugin"));
    if (!runtime.plugin)
    {
        logger->error("plugin descriptor not found");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Check plugin version
    if (runtime.plugin->version != PLUGIN_VERSION)
    {
        logger->error("Version mismatch (plugin={}, expected={})", runtime.plugin->version, PLUGIN_VERSION);
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate required plugin functions
    if (!runtime.plugin->create || !runtime.plugin->destroy)
    {
        logger->error("Plugin validation failed: create and/or destroy functions are missing");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Check that at least one of init_sensors or init_actors is implemented
    if (!runtime.plugin->init_sensors && !runtime.plugin->init_actors)
    {
        logger->error("Plugin validation failed: at least one of init_sensors or init_actors must be implemented");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    if (!runtime.plugin->finit)
    {
        logger->error("Plugin validation failed: finit function is missing");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate configurator function is provided
    if (!runtime.plugin->configurator)
    {
        logger->error("Plugin validation failed: configurator function is missing");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate run function is provided
    if (!runtime.plugin->run)
    {
        logger->error("Plugin validation failed: run function is missing");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate free_config function is provided
    if (!runtime.plugin->free_config)
    {
        logger->error("Plugin validation failed: free_config function is missing");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Load the YAML config file and pass the YAML object to the plugin's configurator
    YAML::Node *yaml;
    try
    {
        yaml = new YAML::Node(YAML::LoadFile(configFile));
    }
    catch (const YAML::Exception& e)
    {
        logger->error("Failed to load config file '{}': {}", configFile, e.what());
            dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Call the plugin's configurator with the YAML object cast to void*
    if (runtime.plugin->configurator)
    {
        runtime.config = runtime.plugin->configurator(static_cast<void*>(yaml)); // or just = yaml;//!
        if (!runtime.config)
        {
            logger->error("Plugin configurator failed to load config from \"{}\"", configFile);
            dlclose(runtime.library);
            runtimes.erase(uuid);
        return false;
    }
    }

    runtime.loaded = true;
    runtime.initialized = false;
    runtime.running = false;

    logger->info("Loaded plugin \"{}\" from {}", uuid, path);
    return true;
}

bool PluginManager::init_plugin(
    const char *uuid)
{
    auto *runtime =
        find_runtime(uuid);

    if (!runtime)
    {
    return false;
}

    if (runtime->initialized)
    {
        logger->error("Plugin \"{}\" is already initialized", uuid);
        return false;
    }

    // Check if plugin is running - cannot re-initialize a running plugin
    if (runtime->running)
    {
        logger->error("Plugin \"{}\" is running. Stop the plugin before re-initializing.", uuid);
        return false;
    }

    if (runtime->plugin->create)
    {
        runtime->plugin->create();
    }

    // if init connections exits, run it
    if (runtime->plugin->init_downstream_connections)
    {
        runtime->downstream_connections_initialized = runtime->plugin->init_downstream_connections(runtime);
    }

    // Create logger for the plugin
    logger = logging.create(std::string("plugin.") + uuid);

    // Initialize sensors if the function is implemented
    if (runtime->plugin->init_sensors)
    {
        runtime->sensors_initialized = runtime->plugin->init_sensors(runtime);
    }

    // Initialize actors if the function is implemented
    if (runtime->plugin->init_actors)
    {
        runtime->actors_initialized = runtime->plugin->init_actors(runtime);
    }

    // Check that at least one of sensors or actors was initialized
    // This allows plugins to initialize either sensors, actors, or both
    if (!runtime->sensors_initialized && !runtime->actors_initialized)
    {
        logger->error("Plugin validation failed during the initialization: both sensors and actors are missing");
        unload_plugin(uuid);
        return false;
    }

    runtime->initialized = true;
    return true;
}

bool PluginManager::start_plugin(
    const char *uuid)
{
    auto *runtime =
        find_runtime(uuid);
    if (!runtime)
    {
        return false;
    }

    // Check if plugin is already started
    if (!runtime->loaded)
    {
        logger->error("Plugin \"{}\" is not loaded", uuid);
        return false;
    }

    // Check if plugin is already started
    if (!runtime->initialized)
    {
        logger->error("Plugin \"{}\" is not initialized", uuid);
        return false;
    }

    // Check if plugin is already started
    if (runtime->running)
    {
        logger->error("Plugin \"{}\" is already running", uuid);
    return false;
}

    runtime->running = true;
    // Call the plugin's run function in a separate thread
    // The run function contains the main execution loop
    if (runtime->plugin->run)
    {
        try {
            threads[uuid] = std::thread(
                [runtime]
                {
                    runtime->plugin->run(runtime);
                }
            );
        } catch (const std::system_error& e) {
            logger->error("Failed to create thread for plugin \"{}\": {}", uuid, e.what());
            return false;
        }
    }

    return true;
}

bool PluginManager::stop_plugin(
    const char *uuid)
{
    auto *runtime =
        find_runtime(uuid);
    if (!runtime)
    {
        return false;
    }

    // Check if plugin is running before stopping
    if (!runtime->running)
    {
        logger->error("Plugin \"{}\" is not running", uuid);
    return false;
}

    runtime->running = false;

    // Wait for the run thread to finish

    if (threads[uuid].joinable())
    {
        threads[uuid].join();
    }

    return true;
}

bool PluginManager::unload_plugin(
    const char *uuid)
{
    auto it = runtimes.find(uuid);
    if (it == runtimes.end())
    {
        return false;
    }

    runtime_t *runtime = &it->second;

    // Check if plugin is running - cannot unload running plugin
    if (runtime->running)
    {
        logger->error("Plugin \"{}\" is still running. Stop the plugin before unloading.", uuid);
    return false;
}

    // Check if plugin is initialized - cannot unload initialized plugin
    if (runtime->initialized)
    {
        logger->error("Plugin \"{}\" is still initialized. Finalize the plugin before unloading.", uuid);
        return false;
    }

    if (runtime->plugin &&
        runtime->plugin->destroy)
    {
        runtime->plugin->destroy(runtime);
    }

    if (runtime->library)
    {
        dlclose(runtime->library);
    }

    if (runtime->loaded){
        runtime->loaded = false;
    }

    runtimes.erase(it);

    return true;
}

std::vector<std::string> PluginManager::get_plugin_capabilities(
    const char *uuid) const
{
    std::vector<std::string> capabilities;

    auto it = runtimes.find(uuid);
    if (it == runtimes.end())
    {
    return capabilities;
}

    const runtime_t &runtime = it->second;
    if (runtime.plugin)
    {
        // Check for required functions
        if (runtime.plugin->create)
        {
            capabilities.push_back("create");
        }
        if (runtime.plugin->destroy)
        {
            capabilities.push_back("destroy");
        }
        if (runtime.plugin->init_sensors)
        {
            capabilities.push_back("init_sensors");
        }
        if (runtime.plugin->init_actors)
        {
            capabilities.push_back("init_actors");
        }
        if (runtime.plugin->finit)
        {
            capabilities.push_back("finit");
        }

        // Check for optional functions
        if (runtime.sensors_initialized)
        {
            capabilities.push_back("sensors");
        }
        if (runtime.actors_initialized)
        {
            capabilities.push_back("actors");
        }
        if (runtime.plugin->run)
        {
            capabilities.push_back("run");
        }
    }

    return capabilities;
}

std::vector<std::string> PluginManager::get_loaded_plugins() const
{
    std::vector<std::string> loaded_plugins;
    for (const auto &pair : runtimes)
    {
        loaded_plugins.push_back(pair.first);
    }

    return loaded_plugins;
}

void PluginManager::print_loaded_plugins() const
{
    if (runtimes.empty())
    {
	logger->info("No plugins loaded");
        return;
    }

    logger->info("Loaded plugins:");
    for (const auto &pair : runtimes)
    {
        logger->info("  UUID: ");
        // Note: config.name is no longer directly accessible in runtime_t
        // It would be in the plugin-specific config object pointed to by plugin->config
        // Add capabilities
        auto capabilities = get_plugin_capabilities(pair.first.c_str());

        if (!capabilities.empty())
	{
	    logger->info(
            "Capabilities: {}",
            fmt::join(capabilities, ", ")
		);
	}
    }
}

// Implement the second version of load_plugin that only takes UUID
bool PluginManager::load_plugin(
    const char *uuid)
{
    // This would typically load from a configuration file based on UUID
    // For now, we'll return false as it's not implemented
    return false;
}


bool PluginManager::init_plugin()
{
    for (const auto &item : runtimes)
    {
        const char *uuid = item.first.c_str();
        init_plugin(uuid);
    }
}

bool PluginManager::finit_plugin()
{
    for (const auto &item : runtimes)
    {
        const char *uuid = item.first.c_str();
        finit_plugin(uuid);
    }
}

bool PluginManager::start_plugin()
{
    for (const auto &item : runtimes)
    {
        const char *uuid = item.first.c_str();
        start_plugin(uuid);
    }
}

bool PluginManager::stop_plugin()
{
    for (const auto &item : runtimes)
    {
        const char *uuid = item.first.c_str();
        stop_plugin(uuid);
    }
}

bool PluginManager::unload_plugin()
{
    for (const auto &item : runtimes)
    {
        const char *uuid = item.first.c_str();
        unload_plugin(uuid);
    }
}

