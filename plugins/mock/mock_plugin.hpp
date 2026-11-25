// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include "plugin.hpp"

version_t* get_version (void);
void* mock_create(void);
bool  mock_destroy(void* runtime);
bool mock_init_sensors(void* runtime);
bool mock_finit(void* runtime);
void* mock_run(void* runtime);
void* mock_configurator(void* config);

extern "C" plugin_interface_t plugin;
