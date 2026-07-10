#include <dlfcn.h>

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
#include <yaml-cpp/yaml.h>


#include "pluginManager.hpp"

namespace
{
using CreatePluginFn = Plugin* (*)(const PluginConfig&);

PluginConfig load_config(const char* filename)
{
    YAML::Node yaml = YAML::LoadFile(filename);

    PluginConfig config;

    config.name = yaml["name"].as<std::string>();

    for (const auto& sensorNode : yaml["sensors"])
    {
        SensorConfig sensor;

        sensor.uuid = sensorNode["uuid"].as<std::string>();
        sensor.topic = sensorNode["topic"].as<std::string>();
        sensor.interval_ms = sensorNode["interval_ms"].as<std::uint32_t>();
        sensor.unit = sensorNode["unit"].as<std::string>();

        config.sensors.push_back(std::move(sensor));
    }

    std::cerr << "Loaded " << config.sensors.size() << " sensors\n";
    return config;
}
}

PluginManager::~PluginManager()
{
    stop();
}

void PluginManager::register_plugin(
    const char* library,
    const char* config)
{
    auto loader =
        std::make_unique<PluginLoader>(
            library,
            config);

    loader->plugin().init();

    m_plugins.push_back(
        std::move(loader));
}

void PluginManager::start()
{
    for (auto& loader : m_plugins)
    {
        loader->plugin().start();
    }
}

void PluginManager::stop()
{
    // Stop all plugins first
    for (auto& loader : m_plugins)
    {
        try
        {
            loader->plugin().stop();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error stopping plugin: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Unknown error stopping plugin" << std::endl;
        }
    }

    // Then finalize all plugins
    for (auto& loader : m_plugins)
    {
        try
        {
            loader->plugin().finit();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error finalizing plugin: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Unknown error finalizing plugin" << std::endl;
        }
    }
}

PluginLoader::~PluginLoader()
{
    // Ensure plugin is stopped before destruction
    if (m_plugin)
    {
        try
        {
            m_plugin->stop();
        }
        catch (...)
        {
            // Ignore errors during cleanup
        }
    }

    m_plugin.reset();

    if (m_handle)
    {
        dlclose(m_handle);
    }
}

#ifdef INCLUDE
PluginLoader::PluginLoader(
    const char* library,
    const char* config_file)
{
    PluginConfig config =
        load_config(config_file);

    auto handle =
        dlopen(
            library,
            RTLD_NOW);

    if (!handle)
    {
        throw std::runtime_error(
            dlerror());
    }

    m_handle = handle;

    dlerror();

    auto create =
        reinterpret_cast<CreatePluginFn>(
            dlsym(
                handle,
                "create_plugin"));

    if (const char* error =
            dlerror())
    {
        throw std::runtime_error(
            error);
    }

    if (!create)
    {
        throw std::runtime_error(
            "create_plugin not found");
    }

    m_plugin.reset(
        create(config));

    if (!m_plugin)
    {
        throw std::runtime_error(
            "plugin creation failed");
    }
}
#endif

PluginLoader::PluginLoader(
    const char* library,
    const char* config_file)
{
    PluginConfig config =
        load_config(config_file);

    auto handle =
        dlopen(
            library,
            RTLD_NOW);

    if (!handle)
    {
        throw std::runtime_error(
            dlerror());
    }

    m_handle = handle;

    dlerror();

    auto create =
        reinterpret_cast<CreatePluginFn>(
            dlsym(
                handle,
                "create_plugin"));

    if (const char* error =
            dlerror())
    {
        throw std::runtime_error(
            error);
    }

    if (!create)
    {
        throw std::runtime_error(
            "create_plugin not found");
    }

    m_plugin.reset(
        create(config));

    if (!m_plugin)
    {
        throw std::runtime_error(
            "plugin creation failed");
    }

    //
    // Optional capabilities
    //

    dlerror();

    m_plugin->m_sense =
        reinterpret_cast<SenseFn>(
            dlsym(
                handle,
                "sense"));

    dlerror();

    m_plugin->m_act =
        reinterpret_cast<ActFn>(
            dlsym(
                handle,
                "act"));

    std::cerr
        << "Plugin features:"
        << (m_plugin->m_sense ? " sense" : "")
        << (m_plugin->m_act ? " act" : "")
        << '\n';
}

Plugin&
PluginLoader::plugin() const
{
    return *m_plugin;
}