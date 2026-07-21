#include "config.hpp"
#include "pluginManager.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

std::atomic_bool running{true};

void signal_handler(
    int signal)
{
    std::cerr
        << "Received signal "
        << signal
        << ", shutting down...\n";

    running = false;
}

int main(
    int argc,
    char** argv)
{
    if (argc != 2)
    {
        std::cerr
            << "usage: "
            << argv[0]
            << " <config.yaml>\n";

        return 1;
    }

    std::signal(
        SIGINT,
        signal_handler);

    std::signal(
        SIGTERM,
        signal_handler);

    auto plugin_configs =
        load_config(argv[1]);

    PluginManager manager;

    for (const auto& conf : plugin_configs)
    {
        std::cout
            << "Loading:  "
            << conf.path << ", "
            << conf.config << ", "
            << conf.uuid << ", "
            << conf.name
            << '\n';

        try
        {
            if (!manager.load_plugin(
                conf.path,
                conf.config,
                conf.uuid))
            {
            std::cerr
                << "Failed to load "
                << conf.path << ", "
                << conf.config << ", "
                << conf.uuid << ", "
                << conf.name
                    << '\n';
        }
    }
        catch (const std::exception& e)
        {
            std::cerr
                << "Exception loading "
                << ": " << e.what()
                << '\n';
        }
    }
    for (const auto& conf : plugin_configs)
    {
        if (!manager.is_plugin_loaded(conf.uuid))
        {
            std::cerr << "Plugin not loaded, skipping init/start: " << conf.uuid << std::endl;
            continue;
        }

        std::cerr << "Initializing the plugin: " << conf.uuid << std::endl;
        if (!manager.init_plugin(conf.uuid))
        {
            std::cerr << "Failed to initialize plugin: " << conf.uuid << std::endl;
            continue;
    }

        std::cerr << "Starting the plugin: " << conf.uuid << std::endl;

        if (!manager.start_plugin(conf.uuid))
        {
            std::cerr << "Failed to start plugin: " << conf.uuid << std::endl;
            // Try to finalize before continuing to next plugin
            manager.finit_plugin(conf.uuid);
            continue;
}
    }

    while (running)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    for (const auto& conf : plugin_configs)
    {
        if (!manager.is_plugin_loaded(conf.uuid))
        {
            continue;
        }

        std::cerr << "Stopping plugin: " << conf.uuid << std::endl;
        manager.stop_plugin(conf.uuid);

        std::cerr << "Finalizing plugin: " << conf.uuid << std::endl;
        manager.finit_plugin(conf.uuid);

        std::cerr << "Unloading plugin: " << conf.uuid << std::endl;
        manager.unload_plugin(conf.uuid);
    }

    return 0;
}
