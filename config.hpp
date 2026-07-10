#pragma once

#include <string>
#include <utility>
#include <vector>

std::vector<
    std::pair<
        std::string,
        std::string>>
load_config(const char *filename);