// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include "pluginManager.hpp"
#include "common.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <yaml-cpp/yaml.h>

#include <dlfcn.h>

#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

bool PluginManager::is_plugin_loaded(
    const char *uuid)
{
    std::lock_guard lock(runtimesMutex);
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

    if (runtime->config)
    {
        free_config(runtime->config);
        runtime->config = nullptr;
    }

    if (runtime->plugin->finit)
    {
        runtime->plugin->finit(runtime);
    }

    runtime->sensors_initialized = false;
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
    stop_loaded_plugin_reporting();

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

    if (telemetryCollectorConfig)
    {
        for (auto &config : telemetryCollectorConfig->plugins)
        {
            std::free(config.path);
            std::free(config.config);
            std::free(config.uuid);
            std::free(config.name);
        }
    }
}

bool PluginManager::load_collector_config(const char *filename)
{
    if (telemetryCollectorConfig)
    {
        logger->error("Collector configuration is already loaded");
        return false;
    }

    auto config = load_tc_config(filename, logger);
    if (!config)
        return false;

    if (config->plugins.empty())
    {
        logger->error("Collector configuration contains no plugins");
        return false;
    }

    telemetryCollectorConfig = std::move(*config);
    return true;
}

const telemetry_collector_config_t &PluginManager::collector_config() const
{
    if (!telemetryCollectorConfig)
        throw std::logic_error("Collector configuration has not been loaded");

    return *telemetryCollectorConfig;
}

bool PluginManager::load_plugin(
    const char *path,
    const char *configFile,
    const char *uuid,
    const char *seanerbusUrl)
{
    std::lock_guard runtimeLock(runtimesMutex);

    // Check if plugin with this UUID is already loaded
    if (runtimes.find(uuid) != runtimes.end())
    {
        logger->error("Plugin with UUID \"{}\" is already loaded", uuid);
    return false;
}

    // Create the entry in the class member map first and work with it directly
    auto& runtime = runtimes[uuid];
    if (!seanerbusUrl || *seanerbusUrl == '\0')
    {
        logger->error("Missing seanerbus URL for plugin \"{}\"", uuid);
        runtimes.erase(uuid);
        return false;
    }
    runtime.seanerbus_url = seanerbusUrl;

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

    // Check that get_version is implemented
    if (!runtime.plugin->get_version)
    {
        logger->error("Plugin validation failed: get_version must be implemented");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate required create & destroy plugin functions
    if (!runtime.plugin->create || !runtime.plugin->destroy)
    {
        logger->error("Plugin validation failed: create and/or destroy functions are missing");
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Check that init_sensors is implemented
    if (!runtime.plugin->init_sensors)
    {
        logger->error("Plugin validation failed: init_sensors implemented");
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

    // Load the YAML config file and pass the YAML object to the plugin's configurator
    YAML::Node *yaml;
    try
    {
        yaml = new YAML::Node(YAML::LoadFile(configFile));
        (*yaml)["seanerbus"]["url"] = seanerbusUrl;
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
        runtime.config = runtime.plugin->configurator(static_cast<void*>(yaml));
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

    // Create logger for the plugin
    logger = logging.create(std::string("plugin.") + uuid);

    // Initialize sensors if the function is implemented
    if (runtime->plugin->init_sensors)
    {
        runtime->sensors_initialized = runtime->plugin->init_sensors(runtime);
    }

    // Check that at least one of sensors or actors was initialized
    // This allows plugins to initialize either sensors, actors, or both
    if (!runtime->sensors_initialized)
    {
        logger->error("Plugin validation failed during the initialization: sensors are missing");
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
    std::lock_guard runtimeLock(runtimesMutex);

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
    std::lock_guard lock(runtimesMutex);
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
        if (runtime.plugin->finit)
        {
            capabilities.push_back("finit");
        }

        // Check for optional functions
        if (runtime.sensors_initialized)
        {
            capabilities.push_back("sensors");
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
    std::lock_guard lock(runtimesMutex);
    std::vector<std::string> loaded_plugins;
    for (const auto &pair : runtimes)
    {
        loaded_plugins.push_back(pair.first);
    }

    return loaded_plugins;
}

void PluginManager::print_loaded_plugins() const
{
    std::lock_guard lock(runtimesMutex);
    if (runtimes.empty())
    {
        logger->info("Currently loaded plugins: none");
        return;
    }

    logger->info("Currently loaded plugins ({}):", runtimes.size());
    for (const auto &entry : runtimes)
    {
        const auto &uuid = entry.first;
        const char *name = uuid.c_str();
        if (telemetryCollectorConfig)
        {
            const auto config = std::find_if(
                telemetryCollectorConfig->plugins.begin(),
                telemetryCollectorConfig->plugins.end(),
                [&uuid](const config_t &pluginConfig)
                {
                    return pluginConfig.uuid && uuid == pluginConfig.uuid;
                });
            if (config != telemetryCollectorConfig->plugins.end() && config->name)
                name = config->name;
        }
        logger->info("  {} ({})", name, uuid);
    }
}

bool PluginManager::start_loaded_plugin_reporting()
{
    if (!telemetryCollectorConfig)
    {
        logger->error(
            "Cannot start plugin status reporting before loading collector configuration");
        return false;
    }

    const auto interval = telemetryCollectorConfig->plugin_status_interval;
    if (interval <= std::chrono::milliseconds::zero())
        return true;

    const auto pollInterval =
        std::min(interval, std::chrono::milliseconds(1000));

    std::lock_guard lock(statusReporterMutex);
    if (statusReporterThread.joinable())
    {
        logger->error("Plugin status reporting is already running");
        return false;
    }

    statusReporterRunning = true;
    try
    {
        statusReporterThread = std::thread(
            [this, interval, pollInterval]
            {
                std::unique_lock reporterLock(statusReporterMutex);
                auto lastReport = std::chrono::steady_clock::now();
                while (statusReporterRunning)
                {
                    if (statusReporterCondition.wait_for(
                            reporterLock,
                            pollInterval,
                            [this]
                            {
                                return !statusReporterRunning;
                            }))
                    {
                        break;
                    }

                    const auto now = std::chrono::steady_clock::now();
                    const auto elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - lastReport);
                    if (elapsed < interval)
                        continue;

                    reporterLock.unlock();
                    print_loaded_plugins();
                    reporterLock.lock();
                    lastReport = now;
                }
            });
    }
    catch (const std::system_error &error)
    {
        statusReporterRunning = false;
        logger->error(
            "Failed to start plugin status reporting: {}",
            error.what());
        return false;
    }

    return true;
}

void PluginManager::stop_loaded_plugin_reporting()
{
    {
        std::lock_guard lock(statusReporterMutex);
        statusReporterRunning = false;
    }
    statusReporterCondition.notify_all();

    if (statusReporterThread.joinable())
        statusReporterThread.join();
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

