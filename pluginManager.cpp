#include "pluginManager.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <yaml-cpp/yaml.h>

#include <dlfcn.h>

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
        std::cerr
            << "Plugin \"" << uuid
            << "\" is still running. Stop the plugin before finalizing." << std::endl;
    return false;
}

    // Check if plugin is initialized before finalizing
    if (!runtime->initialized)
    {
        std::cerr
            << "Plugin \"" << uuid
            << "\" is not initialized" << std::endl;
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
        std::cerr
            << "Plugin with UUID \"" << uuid
            << "\" is already loaded" << std::endl;
    return false;
}

    // Create the entry in the class member map first and work with it directly
    auto& runtime = runtimes[uuid];

    // Load the library
    runtime.library = dlopen(path, RTLD_NOW);
    if (!runtime.library)
    {
        std::cerr
            << "dlopen failed: "
            << dlerror()
            << '\n';
        runtimes.erase(uuid);
        return false;
    }

    // Get the plugin descriptor
    runtime.plugin = reinterpret_cast<plugin_interface_t*>(dlsym(runtime.library, "plugin"));
    if (!runtime.plugin)
    {
        std::cerr
            << "plugin descriptor not found\n";
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Check plugin version
    if (runtime.plugin->version != PLUGIN_VERSION)
    {
        std::cerr
            << "Version mismatch (plugin="
            << runtime.plugin->version
            << ", expected="
            << PLUGIN_VERSION
            << ")\n";
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate required plugin functions
    if (!runtime.plugin->create || !runtime.plugin->destroy)
    {
        std::cerr
            << "Plugin validation failed: create and/or destroy functions are missing\n";
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Check that at least one of init_sensors or init_actors is implemented
    if (!runtime.plugin->init_sensors && !runtime.plugin->init_actors)
    {
        std::cerr
            << "Plugin validation failed: at least one of init_sensors or init_actors must be implemented\n";
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    if (!runtime.plugin->finit)
    {
        std::cerr
            << "Plugin validation failed: finit function is missing\n";
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate configurator function is provided
    if (!runtime.plugin->configurator)
    {
        std::cerr
            << "Plugin validation failed: configurator function is missing\n";
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate run function is provided
    if (!runtime.plugin->run)
    {
        std::cerr
            << "Plugin validation failed: run function is missing\n";
        dlclose(runtime.library);
        runtimes.erase(uuid);
        return false;
    }

    // Validate free_config function is provided
    if (!runtime.plugin->free_config)
    {
        std::cerr
            << "Plugin validation failed: free_config function is missing\n";
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
        std::cerr
            << "Failed to load config file '" << configFile << "': " << e.what() << '\n';
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
        std::cerr
                << "Plugin configurator failed to load config from \""
                << configFile
                << "\"\n";
            dlclose(runtime.library);
            runtimes.erase(uuid);
        return false;
    }
    }

    runtime.loaded = true;
    runtime.initialized = false;
    runtime.running = false;

    std::cerr
        << "Loaded plugin \""
        << uuid
        << "\" from "
        << path
        << std::endl;
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
        std::cerr
            << "Plugin \"" << uuid
            << "\" is already initialized" << std::endl;
        return false;
    }

    // Check if plugin is running - cannot re-initialize a running plugin
    if (runtime->running)
    {
        std::cerr
            << "Plugin \"" << uuid
            << "\" is running. Stop the plugin before re-initializing." << std::endl;
        return false;
    }

    if (runtime->plugin->create)
    {
        runtime->plugin->create();
    }


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
        std::cerr
            << "Plugin validation failed during the initialization: both sensors and actors are missing" << std::endl;
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
        std::cerr
            << "Plugin \"" << uuid
            << "\" is not loaded" << std::endl;
        return false;
    }

    // Check if plugin is already started
    if (!runtime->initialized)
    {
        std::cerr
            << "Plugin \"" << uuid
            << "\" is not initialized" << std::endl;
        return false;
    }

    // Check if plugin is already started
    if (runtime->running)
    {
        std::cerr
            << "Plugin \"" << uuid
            << "\" is already running" << std::endl;
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
            std::cerr << "Failed to create thread for plugin \"" << uuid << "\": " << e.what() << std::endl;
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
        std::cerr
            << "Plugin \"" << uuid
            << "\" is not running" << std::endl;
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
        std::cerr
            << "Plugin \"" << uuid
            << "\" is still running. Stop the plugin before unloading." << std::endl;
    return false;
}

    // Check if plugin is initialized - cannot unload initialized plugin
    if (runtime->initialized)
    {
        std::cerr
            << "Plugin \"" << uuid
            << "\" is still initialized. Finalize the plugin before unloading." << std::endl;
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
        std::cout << "No plugins loaded" << std::endl;
        return;
    }

    std::cout << "Loaded plugins:" << std::endl;
    for (const auto &pair : runtimes)
    {
        std::cout << "  UUID: " << pair.first;
        // Note: config.name is no longer directly accessible in runtime_t
        // It would be in the plugin-specific config object pointed to by plugin->config
        // Add capabilities
        auto capabilities = get_plugin_capabilities(pair.first.c_str());
        if (!capabilities.empty())
        {
            std::cout << ", Capabilities: ";
            for (size_t i = 0; i < capabilities.size(); ++i)
            {
                std::cout << capabilities[i];
                if (i < capabilities.size() - 1)
                {
                    std::cout << ", ";
                }
            }
        }

        std::cout << std::endl;
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