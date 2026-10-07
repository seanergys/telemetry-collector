#pragma once

#include <string>
#include <utility>
#include <vector>
#include <memory>

#include "logger.hpp"
#include "plugin.hpp"

std::vector<config_t> load_tc_config(const char* filename, std::shared_ptr<spdlog::logger> logger);
