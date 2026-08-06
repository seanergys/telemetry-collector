#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include "../plugin.hpp"
#include "../types.hpp"

/* lifecycle */
void *snmp_create(void);
bool snmp_destroy(void *runtime);
bool snmp_init_sensors(void *runtime);
bool snmp_finit(void *runtime);
void *snmp_run(void *runtime);

/* config loading */
void *snmp_configurator(void *config);
bool snmp_free_config(void *config);

typedef struct sensor_attribute
{
    char *uuid;
    char *topic;
    std::chrono::milliseconds interval_ms;
    char *unit;
    char *description;
    char *filter;
    double factor = 1;
    double offset = 0;
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
    oid         _oid[MAX_OID_LEN];
    size_t      _oidLen;

} sensor_attribute_t;

std::map<std::string, sensor_attribute_t *> configs; // uuid -> configs
std::map<std::string, sensor_t *> sensors;           // uuid -> sensors

/* exported plugin descriptor */
extern "C" plugin_interface_t plugin;