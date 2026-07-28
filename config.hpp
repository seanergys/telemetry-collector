#pragma once

#include <string>
#include <utility>
#include <vector>

#include "plugin.hpp"

std::vector<config_t> load_config(const char* filename);