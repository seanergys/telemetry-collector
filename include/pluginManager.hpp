// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include "logger.hpp"
#include "config.hpp"
#include "plugin.hpp"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
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
        const char* uuid,
        const char* seanerbusUrl);

    bool load_plugin(
        const char* uuid);

    bool load_collector_config(const char* filename);
    const telemetry_collector_config_t& collector_config() const;

    bool start_plugin(
        const char* uuid);

    bool start_plugin(); // for all

    bool stop_plugin(
        const char* uuid);

    bool stop_plugin(); // for all

    bool init_plugin(
        const char* uuid);

    bool init_plugin(); // for all

    bool finit_plugin(
        const char* uuid);

    bool finit_plugin(); // for all

    bool unload_plugin(
        const char* uuid);

    bool unload_plugin(); // for all

    bool is_plugin_loaded(
        const char* uuid);

    std::vector<std::string> get_plugin_capabilities(
        const char* uuid) const;

    std::vector<std::string> get_loaded_plugins() const;

    void print_loaded_plugins() const;
    bool start_loaded_plugin_reporting();
    void stop_loaded_plugin_reporting();

private:
    std::map<std::string, runtime_t> runtimes; // uuid -> runtime_t
    mutable std::mutex runtimesMutex;
    std::map<std::string, std::thread> threads; // uuid -> runtime_t
    std::mutex statusReporterMutex;
    std::condition_variable statusReporterCondition;
    bool statusReporterRunning = false;
    std::thread statusReporterThread;
    std::optional<telemetry_collector_config_t> telemetryCollectorConfig;
    runtime_t* find_runtime(
        const char* uuid);
    Logger logging; 
    std::shared_ptr<spdlog::logger> logger = logging.create("telemetry-collector::plugin-manager");;
};
