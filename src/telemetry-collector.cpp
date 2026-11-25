// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include "logger.hpp"
#include "pluginManager.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

std::atomic_bool running{true};

// Global logger instance
std::shared_ptr<spdlog::logger> logger;

PluginManager manager;

void signal_handler(
    int)
{
    running = false;
}

int main(
    int argc,
    char **argv)
{
    Logger logging;
    logger = logging.create("telemetry-collector");

    if (argc != 2)
    {
        logger->set_pattern("%v");
        logger->error("usage: {} <config.yaml>", argv[0]);
        return 1;
    }

    std::signal(
        SIGINT,
        signal_handler);

    std::signal(
        SIGTERM,
        signal_handler);

    if (!manager.load_collector_config(argv[1]))
    {
        return 1;
    }

    const auto &plugin_configs =
        manager.collector_config().plugins;

    std::vector<std::string> loaded_plugins;
    std::vector<std::string> initialized_plugins;
    std::vector<std::string> running_plugins;

    for (const auto &conf : plugin_configs)
    {
        try
        {
            logger->info("Loading {}, {}, {}, {}", conf.path, conf.config, conf.uuid, conf.name);

            if (!manager.load_plugin(
                    conf.path,
                    conf.config,
                    conf.uuid,
                    conf.seanerbus_url.c_str()))
            {
                logger->error("Failed to load {}, {}, {}, {}", conf.path, conf.config, conf.uuid, conf.name);
                continue;
            }
            loaded_plugins.emplace_back(conf.uuid);

            logger->info("Initializing the plugin: {}", conf.uuid);
            if (!manager.init_plugin(conf.uuid))
            {
                logger->error("Failed to initialize plugin: {}", conf.uuid);
                continue;
            }
            initialized_plugins.emplace_back(conf.uuid);

            logger->info("Starting the plugin: {}", conf.uuid);
            if (!manager.start_plugin(conf.uuid))
            {
                logger->error("Failed to start plugin: {}", conf.uuid);
                continue;
            }
            running_plugins.emplace_back(conf.uuid);
        }
        catch (const std::exception &e)
        {
            logger->error(
                "Plugin '{}' ({}) failed: {}",
                conf.name,
                conf.uuid,
                e.what());
        }
    }

    manager.start_loaded_plugin_reporting();

    while (running)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    manager.stop_loaded_plugin_reporting();

    for (const auto &uuid : running_plugins)
    {
        logger->info("Stopping plugin: {}", uuid);
        manager.stop_plugin(uuid.c_str());
    }

    for (const auto &uuid : initialized_plugins)
    {
        logger->info("Finalizing plugin: {}", uuid);
        manager.finit_plugin(uuid.c_str());
    }

    for (const auto &uuid : loaded_plugins)
    {
        if (!manager.is_plugin_loaded(uuid.c_str()))
        {
            continue;
        }

        logger->info("Unloading plugin: {}", uuid);
        manager.unload_plugin(uuid.c_str());
    }

    return 0;
}
