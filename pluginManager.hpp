#pragma once

#include "plugin.hpp"

#include <map>
#include <string>
#include <thread>
#include <vector>

class PluginManager
{
public:
    PluginManager() = default;
    ~PluginManager();

    bool load_plugin(
        const char* path,
        const char* configFile,
        const char* uuid);

    bool load_plugin(
        const char* uuid);

    bool start_plugin(
        const char* uuid);

    bool stop_plugin(
        const char* uuid);

    bool init_plugin(
        const char* uuid);

    bool finit_plugin(
        const char* uuid);

    bool unload_plugin(
        const char* uuid);

    bool is_plugin_loaded(
        const char* uuid);

    std::vector<std::string> get_plugin_capabilities(
        const char* uuid) const;

    std::vector<std::string> get_loaded_plugins() const;

    void print_loaded_plugins() const;

private:
    std::map<std::string, runtime_t> runtimes; // uuid -> runtime_t
    std::vector<std::thread> threads;

    runtime_t* find_runtime(
        const char* uuid);
};