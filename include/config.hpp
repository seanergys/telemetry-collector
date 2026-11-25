// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <memory>

#include "logger.hpp"
#include "plugin.hpp"

struct telemetry_collector_config_t
{
    std::vector<config_t> plugins;
    std::chrono::milliseconds plugin_status_interval;
};

std::optional<telemetry_collector_config_t> load_tc_config(
    const char* filename,
    std::shared_ptr<spdlog::logger> logger);
