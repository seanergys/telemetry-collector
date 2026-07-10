#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "sensor.hpp"

#include <seanerbus/client/connection.hpp>
#include <seanerbus/msg/time_value_v1.hpp>

class Payload
{
public:
    Payload()
        : timestamp(0)
        , value(0.0)
    {
    }

    std::uint64_t timestamp;
    double value;
};

class SeanerbusWrapper
{
public:
    void send(std::string uuid, Payload payload)
    {
        std::cout << "[" << uuid << "]: timestamp=" << payload.timestamp << ", value=" << payload.value << '\n';

        std::string broker = "localhost";
        std::string port = "5398";

        seanerbus::client::Connection conn(broker.c_str(), port.c_str());

        auto addr = seanerbus::Address{seanerbus::Uuid{uuid}};

        seanerbus::msg::TimeValueV1 tv{
            payload.timestamp,
            payload.value
        };

        conn.publish(addr, tv.to_capnp());
    }
};

struct PluginConfig
{
    std::string name;
    std::vector<SensorConfig> sensors;
};

class Plugin
{
public:
    explicit Plugin(const PluginConfig& config);
    virtual ~Plugin();

    const std::string& name() const;

    // Lifecycle methods
    virtual void init() = 0;
    virtual void finit() = 0;

    using SenseFn =
    std::vector<std::pair<std::string, Payload>>
    (*)(Plugin*);

    using ActFn =
    void (*)(Plugin*);

    SenseFn m_sense = nullptr;
    ActFn m_act = nullptr;

    // Runtime functions
    void start();
    void stop();

    bool can_sense() const
    {
        return m_sense != nullptr;
    }

    bool can_act() const
    {
        return m_act != nullptr;
    }

    std::vector<std::pair<std::string, Payload>> sense()
    {
        return m_sense(this);
    }

    void act()
    {
        m_act(this);
    }


protected:
    const PluginConfig& config() const;

private:
    PluginConfig m_config;
    std::atomic<bool> m_running{false};
    std::thread m_sense_thread;
};

using SenseFn =
std::vector<std::pair<std::string, Payload>>
(*)(Plugin*);

using ActFn =
void (*)(Plugin*);

extern "C" Plugin* create_plugin(const PluginConfig& config);