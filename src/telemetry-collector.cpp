#include "logger.hpp"
#include "config.hpp"
#include "pluginManager.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

std::atomic_bool running{true};

// Global logger instance
std::shared_ptr<spdlog::logger> logger;

PluginManager manager;

void signal_handler(
    int signal)
{
    logger->error("Received signal {}, shutting down...", signal);

    logger->info("Stopping plugins");
    manager.stop_plugin();

    logger->info("Finalizing plugins");
    manager.finit_plugin();

    logger->info("Unloading plugins");
    manager.unload_plugin();

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
        logger->error("usage: {} <config.yaml>", argv[0]);
        return 1;
    }

    std::signal(
        SIGINT,
        signal_handler);

    std::signal(
        SIGTERM,
        signal_handler);

    auto plugin_configs =
        load_tc_config(argv[1], logger);

    for (const auto &conf : plugin_configs)
    {
        try
        {
	    logger->info("Loading {}, {}, {}, {}", conf.path, conf.config, conf.uuid, conf.name);

            if (!manager.load_plugin(conf.path, conf.config, conf.uuid))
            {
                logger->error("Failed to load {}, {}, {}, {}", conf.path, conf.config, conf.uuid, conf.name);
            }

            logger->info("Initializing the plugin: {}", conf.uuid);
            if (!manager.init_plugin(conf.uuid))
            {
                logger->error("Failed to initialize plugin: {}", conf.uuid);
            }

            logger->info("Starting the plugin: {}", conf.uuid);
            if (!manager.start_plugin(conf.uuid))
            {
                logger->error("Failed to start plugin: {}", conf.uuid);
            }
        }
        catch (const std::exception &e)
        {
            logger->error("Exception: {}", e.what());
        }
    }

    while (running)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    for (const auto &conf : plugin_configs)
    {
        logger->info("Stopping plugin: {}", conf.uuid);
        manager.stop_plugin(conf.uuid);

        logger->info("Finalizing plugin: {}", conf.uuid);
        manager.finit_plugin(conf.uuid);

        logger->info("Unloading plugin: {}", conf.uuid);
        manager.unload_plugin(conf.uuid);
    }

    return 0;
}