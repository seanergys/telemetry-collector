#pragma once

#include "../plugin.hpp"

#include <chrono>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class TesterPlugin : public Plugin
{
public:
    explicit TesterPlugin(
        const PluginConfig& config);

    void init() override;

    void finit() override;

    std::vector<std::pair<std::string, Payload>>
    sense_impl();

private:
    double getValue(
        const std::string& sensor_uuid);

    std::unordered_map<
        std::string,
        double>
        m_values;

    std::unordered_map<
        std::string,
        std::chrono::steady_clock::time_point>
        m_next_due;
};