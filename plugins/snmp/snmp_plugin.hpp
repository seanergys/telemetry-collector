// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include "logger.hpp"
#include "plugin.hpp"
#include "types.hpp"
#include <optional>

/* lifecycle */
void *snmp_create(void);
bool snmp_destroy(void *runtime);
bool snmp_init_sensors(void *runtime);
bool snmp_initlib(void *runtime);
bool snmp_finit(void *runtime);

void *snmp_run(void *runtime);

/* config loading */
void *snmp_configurator(void *config);

typedef struct sensor_attribute : generic_sensor_attribute_t
{
    char *connection;
    char *type;
    std::string snmpCommunity;
    std::string oidPrefix;
    std::string username;
    std::string authKey;
    std::string privKey;
    oid *authProto;
    oid *privProto;
    size_t authProtoLen;
    size_t privProtoLen;
    int securityLevel;
    long int version;
    struct snmp_session snmpSession;
    void *snmpSessp;

    std::string oid_suffix;
    std::string oid_prefix;
    oid _oid[MAX_OID_LEN];
    size_t _oidLen;

} sensor_attribute_t;

struct SensorReading
{
    uint64_t time = 0;
    double value = 0.0;
};

std::map<std::string, sensor_attribute_t *> configs; // uuid -> configs
std::map<std::string, sensor_t *> sensors;           // uuid -> sensors

/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;
