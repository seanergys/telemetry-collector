#include "plugin.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

Plugin::Plugin(const PluginConfig& config)
    : m_config(config)
{
}

Plugin::~Plugin()
{
    stop();
}

const PluginConfig& Plugin::config() const
{
    return m_config;
}

void Plugin::start()
{
    if (m_running)
    {
        return;
    }

    m_running = true;

    m_sense_thread = std::thread([this]
    {
                while (m_running)
                {
                    try
                    {
                auto measurements = sense();

                std::cerr << "sense() returned " << measurements.size() << " measurements\n";
                        for (const auto& [uuid, payload] : measurements)
                        {
                            try
                            {
                        std::cerr << "sending " << uuid << '\n';
                        SeanerbusWrapper seanerbus;
                                seanerbus.send(uuid, payload);
                            }
                            catch (const std::exception& e)
                            {
                                std::cerr
                                    << "Failed to send sensor "
                                    << uuid
                                    << ": "
                                    << e.what()
                                    << '\n';
                            }
                            catch (...)
                            {
                                std::cerr
                                    << "Failed to send sensor "
                                    << uuid
                                    << ": unknown exception\n";
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr
                            << "sense() failed: "
                            << e.what()
                            << '\n';
                    }
                    catch (...)
                    {
                        std::cerr
                            << "sense() failed: unknown exception\n";
                    }

                    // Use a shorter sleep interval to allow faster shutdown detection
                    if (m_running)
                    {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(
                                100));
                    }
                }
            });
}

void Plugin::stop()
{
    m_running = false;

    if (m_sense_thread.joinable())
    {
        m_sense_thread.join();
    }
}

const std::string&
Plugin::name() const
{
    return m_config.name;
}