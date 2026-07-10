#include "config.hpp"
#include "pluginManager.hpp"

#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <iostream>

std::atomic<bool> running{true};

void signal_handler(int signal)
{
    std::cerr << "Received signal " << signal << ", shutting down gracefully...\n";
    running = false;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        return 1;
    }

    // Setup signal handlers using C-style signal functions
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    auto plugins = load_config(argv[1]);

    PluginManager manager;

    for (const auto& [library, config] : plugins)
    {
        manager.register_plugin(library.c_str(), config.c_str());
    }

    manager.start();

    // Wait for shutdown signal
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::cerr << "Shutting down...\n";
    manager.stop();

    return 0;
}