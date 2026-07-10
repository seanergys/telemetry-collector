#pragma once

#include "plugin.hpp"

#include <memory>
#include <vector>

class PluginLoader
{
public:
    PluginLoader(const char* library, const char* config);
    ~PluginLoader();

    Plugin& plugin() const;

private:
    void* m_handle{};
    std::unique_ptr<Plugin> m_plugin;
};

class PluginManager
{
public:
    ~PluginManager();

    void register_plugin(
        const char* library,
        const char* config);

    void start();
    void stop();

private:
    std::vector<
        std::unique_ptr<PluginLoader>
    > m_plugins;
};