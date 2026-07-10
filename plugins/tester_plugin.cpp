#include "tester_plugin.hpp"

#include <ctime>
#include <iostream>

TesterPlugin::TesterPlugin(
    const PluginConfig& config)
    : Plugin(config)
{
    auto now =
        std::chrono::steady_clock::now();

    for (const auto& sensor :
         this->config().sensors)
    {
        m_next_due.emplace(
            sensor.uuid,
            now);
    }
}

double TesterPlugin::getValue(
    const std::string& sensor_uuid)
{
    auto& value =
        m_values[sensor_uuid];

    value += 1.0;

    return value;
}

void TesterPlugin::init()
{
    std::cout
        << "[test] init\n";
}

std::vector<
    std::pair<std::string, Payload>>
TesterPlugin::sense_impl()
{
    std::cerr
        << "configured sensors="
        << config().sensors.size()
        << '\n';

    std::vector<
        std::pair<std::string, Payload>>
        measurements;

    auto now =
        std::chrono::steady_clock::now();

    for (const auto& sensor :
         config().sensors)
    {
        std::cerr
            << "sensor="
            << sensor.uuid
            << '\n';

        auto& next_due =
            m_next_due[sensor.uuid];

        if (now < next_due)
        {
            continue;
        }

        next_due =
            now +
            std::chrono::milliseconds(
                sensor.interval_ms);

        Payload payload;

        payload.timestamp =
            std::time(nullptr);

        payload.value =
            getValue(sensor.uuid);

        measurements.emplace_back(
            sensor.uuid,
            std::move(payload));
    }

    return measurements;
}

void TesterPlugin::finit()
{
    std::cout
        << "[test] finit\n";
}

extern "C"
std::vector<
    std::pair<std::string, Payload>>
sense(Plugin* plugin)
{
    return static_cast<TesterPlugin*>(plugin)
        ->sense_impl();
}

extern "C"
Plugin* create_plugin(
    const PluginConfig& config)
{
    return new TesterPlugin(config);
}