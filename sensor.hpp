#pragma once
#include<vector>
#include<string>

struct SensorConfig
{
    std::string uuid;
    std::string topic;
    std::uint32_t interval_ms;
    std::string unit;
};