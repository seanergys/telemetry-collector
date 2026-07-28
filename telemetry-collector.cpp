#include "config.hpp"
#include "pluginManager.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

std::atomic_bool running{true};

PluginManager manager;

void signal_handler(
    int signal)
{
    std::cerr
        << "Received signal "
        << signal
        << ", shutting down...\n";

        std::cerr << "Stopping plugins " << std::endl;
        manager.stop_plugin();

        std::cerr << "Finalizing plugins " << std::endl;
        manager.finit_plugin();

        std::cerr << "Unloading plugins " << std::endl;
        manager.unload_plugin();

    running = false;
}

int main(
    int argc,
    char **argv)
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

    for (const auto &conf : plugin_configs)
    {
        try
        {
            std::cout << "Loading" << conf.path << ", " << conf.config << ", " << conf.uuid << ", " << conf.name << std::endl;;
            if (!manager.load_plugin(conf.path, conf.config, conf.uuid))
            {
                std::cerr << "Failed to load " << conf.path << ", " << conf.config << ", " << conf.uuid << ", " << conf.name << std::endl;;
            }

            std::cerr << "Initializing the plugin: " << conf.uuid << std::endl;
            if (!manager.init_plugin(conf.uuid))
            {
                std::cerr << "Failed to initialize plugin: " << conf.uuid << std::endl;
            }

            std::cerr << "Starting the plugin: " << conf.uuid << std::endl;
            if (!manager.start_plugin(conf.uuid))
            {
                std::cerr << "Failed to start plugin: " << conf.uuid << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr
                << "Exception "
                << ": " << e.what()
                << std::endl;;
        }
    }

    while (running)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    for (const auto &conf : plugin_configs)
    {
        std::cerr << "Stopping plugin: " << conf.uuid << std::endl;
        manager.stop_plugin(conf.uuid);

        std::cerr << "Finalizing plugin: " << conf.uuid << std::endl;
        manager.finit_plugin(conf.uuid);

        std::cerr << "Unloading plugin: " << conf.uuid << std::endl;
        manager.unload_plugin(conf.uuid);
    }

    return 0;
}
