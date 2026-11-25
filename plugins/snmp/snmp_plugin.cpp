// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <algorithm>
#include <yaml-cpp/yaml.h>
#include <string>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/mib_api.h>

#include "snmp_plugin.hpp"
#include "../plugin_common.hpp"

#include <seanerbus/client/connection.hpp>
#include "seanerbus/client/simple_event_loop.hpp"
#include <seanerbus/helpers/resolve.hpp>

static std::map<std::string, SensorReading *> sensor_values;
static Logger logging;
static std::shared_ptr<spdlog::logger> logger;

version_t* get_version (void){
    return nullptr;
}

void *sensor_sense(void *sensor);

void *snmp_create(void)
{
    if (!logger)
        logger = logging.create("telemetry-collector::snmp");
    // Return a pointer to the plugin_interface_t object as the instance
    return &plugin;
}

bool snmp_destroy(void *runtime)
{
    // The plugin_interface_t object is statically allocated, so we don't free it
    // Just clean up any resources if needed
    false;
}

void setVersion(const std::string &version, sensor_attribute_t *conf)
{
    int vers = stoi(version);
    switch (vers)
    {
    case 1:
        conf->version = SNMP_VERSION_1;
        break;
    case 2:
        conf->version = SNMP_VERSION_2c;
        break;
    case 3:
        conf->version = SNMP_VERSION_3;
        break;
    default:
        conf->version = -1;
    }
}

void applyFields(const YAML::Node &node, sensor_attribute_t *conf)
{
    plugin_common::applyCommonSensorFields(
        node, conf, plugin_common::UuidPolicy::GenerateWhenInvalid);
    if (node["connection"])
        plugin_common::setCharField(conf->connection, node["connection"].as<std::string>());
    if (node["community"])
        conf->snmpCommunity = node["community"].as<std::string>();
    if (node["oidPrefix"])
        conf->oidPrefix = node["oidPrefix"].as<std::string>();
    if (node["username"])
        conf->username = node["username"].as<std::string>();
    if (node["authKey"])
        conf->authKey = node["authKey"].as<std::string>();
    if (node["privKey"])
        conf->privKey = node["privKey"].as<std::string>();
    if (node["authProto"])
        conf->authProto = (oid *)node["authProto"].as<oid>();
    if (node["privProto"])
        conf->privProto = (oid *)node["privProto"].as<oid>();
    if (node["authProtoLen"])
        conf->authProtoLen = node["authProtoLen"].as<size_t>();
    if (node["privProtoLen"])
        conf->privProtoLen = node["privProtoLen"].as<size_t>();
    if (node["securityLevel"])
        conf->securityLevel = node["securityLevel"].as<int>();
    if (node["version"])
        setVersion(node["version"].as<std::string>(), conf);
    if (node["type"])
        plugin_common::setCharField(conf->type, node["type"].as<std::string>());
    if (node["oid_prefix"])
        conf->oid_prefix = node["oid_prefix"].as<std::string>();
    if (node["oid_suffix"])
        conf->oid_suffix = node["oid_suffix"].as<std::string>();

}

bool setOID(sensor_attribute_t *conf)
{
    const std::string tmp_oid = conf->oid_prefix + conf->oid_suffix;

    size_t newoidnamelen = MAX_OID_LEN;
    if (read_objid(tmp_oid.c_str(), conf->_oid, &newoidnamelen))
    {
        conf->_oidLen = newoidnamelen;
        return true;
    }

    conf->_oidLen = 0;
    return false;
}

void discardSensorConfig(sensor_attribute_t *conf)
{
    delete[] conf->uuid;
    delete[] conf->topic;
    delete[] conf->unit;
    delete[] conf->description;
    delete[] conf->filter;
    delete[] conf->connection;
    delete[] conf->type;
    delete conf;
}

void *snmp_configurator(void *configPtr)
{
    void *result =
        plugin_common::commonConfigurationProcedure<sensor_attribute_t>(
            configPtr, logger, logging, "telemetry-collector::snmp", applyFields,
            ::configs);

    for (auto sensor = ::configs.begin(); sensor != ::configs.end();)
    {
        auto *conf = sensor->second;
        bool valid = true;
        if (!conf->connection || conf->connection[0] == '\0')
        {
            logger->warn(
                "Skipping SNMP sensor '{}': connection is missing",
                sensor->first);
            valid = false;
        }
        if (conf->oid_prefix.empty() && conf->oid_suffix.empty())
        {
            logger->warn(
                "Skipping SNMP sensor '{}': OID is missing",
                sensor->first);
            valid = false;
        }

        if (valid)
        {
            ++sensor;
            continue;
        }

        discardSensorConfig(conf);
        sensor = ::configs.erase(sensor);
    }

    return result;
}

void setup_snmp_session(sensor_attribute_t *conf)
{

    snmp_sess_init(&conf->snmpSession);
    int liberr, syserr;
    char *errstr;
    snmp_error(&conf->snmpSession, &liberr, &syserr, &errstr);
    if (liberr || syserr)
    {
        logger->error("SNMP: Error initializing session: {}", std::string(errstr));
    }
    free(errstr);
    conf->snmpSession.version = conf->version;
    conf->snmpSession.peername = strdup(conf->connection);

    if ((conf->version == SNMP_VERSION_1) || (conf->version == SNMP_VERSION_2c))
    {
        conf->snmpSession.community = (u_char *)strdup(conf->snmpCommunity.c_str());
        conf->snmpSession.community_len = conf->snmpCommunity.length();
    }
    else if (conf->version == SNMP_VERSION_3)
    {
        conf->snmpSession.community = NULL;
        conf->snmpSession.community_len = 0;

        /* set the SNMPv3 user name */
        conf->snmpSession.securityName = strdup(conf->username.c_str());
        conf->snmpSession.securityNameLen = conf->username.length();

        /* set the security level */
        conf->snmpSession.securityLevel = conf->securityLevel;

        if (conf->snmpSession.securityLevel == SNMP_SEC_LEVEL_NOAUTH)
        {
            // no authentication, no privacy
            // nothing to do
        }
        else if (conf->snmpSession.securityLevel == SNMP_SEC_LEVEL_AUTHNOPRIV || conf->snmpSession.securityLevel == SNMP_SEC_LEVEL_AUTHPRIV)
        {
            // authentication...
            conf->snmpSession.securityAuthKeyLen = USM_AUTH_KU_LEN;

            // set the authentication method
            conf->snmpSession.securityAuthProto = conf->authProto;
            conf->snmpSession.securityAuthProtoLen = conf->authProtoLen; // sizeof(_authProto)/sizeof(oid);

            /* set the authentication key to a hashed version of our
            _authKey (which must be at least 8 characters long) */
            if (generate_Ku(conf->snmpSession.securityAuthProto, conf->snmpSession.securityAuthProtoLen, (u_char *)conf->authKey.c_str(), conf->authKey.length(),
                            conf->snmpSession.securityAuthKey, &conf->snmpSession.securityAuthKeyLen) != SNMPERR_SUCCESS)
            {
                logger->error("SNMP: Error generating Ku from authentication key");
            }

            if (conf->snmpSession.securityLevel == SNMP_SEC_LEVEL_AUTHPRIV)
            {
                //...with privacy
                conf->snmpSession.securityPrivKeyLen = USM_PRIV_KU_LEN;

                // set the privacy method (aka encryption)
                conf->snmpSession.securityPrivProto = conf->privProto;
                conf->snmpSession.securityPrivProtoLen = conf->privProtoLen;

                /* set the privacy key to a hashed version of our
                _privKey (which must be at least 8 characters long) */
                if (generate_Ku(conf->snmpSession.securityAuthProto, conf->snmpSession.securityAuthProtoLen, (u_char *)conf->privKey.c_str(),
                                conf->privKey.length(), conf->snmpSession.securityPrivKey, &conf->snmpSession.securityPrivKeyLen) != SNMPERR_SUCCESS)
                {
                    logger->error("SNMP: Error generating Ku from privacy key");
                }
            }
        }
        else
        {
            logger->error("SNMP security level unknown!");
        }
    }
    else
    {
        logger->error("SNMP Version {} not supported!", conf->version);
    }
}

bool snmp_init_sensors(void *runtime)
{

    init_snmp("SNMPplugin");

    // Get the plugin config from runtime

    // Connect to the server.
    auto conn = plugin_common::makeConnection(runtime);

    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));

    for (const auto &[uuid, conf] : configs)
    {
        if (!setOID(conf))
        {
            logger->warn("Skipping SNMP sensor '{}': OID is invalid", uuid);
            continue;
        }

        setup_snmp_session(conf);

        plugin_common::initializeSensor(
            uuid, conf, sensor_sense, ::sensors, sensor_values);
        plugin_common::addHostname(conf->nameserver_parts, hostname);

        const auto nameserver_parts_str =
            plugin_common::nameserverPartsToString(conf->nameserver_parts);
        logger->info("Registering sensor {{}}: {}", nameserver_parts_str, conf->uuid);
        plugin_common::registerSensor(conn, conf->nameserver_parts, conf->uuid);
    }

    seanerbus::client::SimpleEventLoop sl(conn);
    sl.run();

    conn.shutdown();
    return true; // We're using the global map, not returning a pointer to sensors
}

bool snmp_initlib(void *runtime)
{
    init_snmp("SNMPplugin");
    return true;
}

bool snmp_finit(void *runtime)
{

    // Clear the map
    for (auto &pair : ::configs)
    {
        sensor_attribute_t *conf = pair.second;
        if (conf)
        {
            delete conf->uuid;
            delete conf->topic;
            delete conf->unit;
            delete conf;
        }
    }

    // Clean up the global sensors map
    for (auto &pair : ::sensors)
    {
        sensor_attribute_t *conf = pair.second->conf;
        if (conf)
        {
            delete conf->uuid;
            delete conf->topic;
            delete conf->unit;
            delete conf;
        }

        sensor_t *sensor = pair.second;
        if (sensor)
            delete sensor;
    }
    ::sensors.clear();
    ::configs.clear();

    for (auto &pair : sensor_values)
    {
        if (pair.second)
            delete pair.second;
    }
    // Clear the sensor_values map as well
    sensor_values.clear();

    // Reset num_sensors in runtime
    // if (runtime) {
    //    ((runtime_t*)runtime)->num_sensors = 0;
    //}

    return true;
}

bool open(sensor_attribute_t *conf)
{
    conf->snmpSessp = snmp_sess_open(&conf->snmpSession);
    int liberr, syserr;
    char *errstr;
    snmp_error(&conf->snmpSession, &liberr, &syserr, &errstr);
    if (liberr || syserr)
    {
        logger->error("SNMP-open: {}", errstr);
        free(errstr);
        return false;
    }
    free(errstr);
    return true;
}

void close(sensor_attribute_t *conf)
{
    if (conf->snmpSessp)
    {
        snmp_sess_close(conf->snmpSessp);
        conf->snmpSessp = NULL;
    }
}

int64_t get(sensor_attribute_t *conf)
{
    const oid *const OID = conf->_oid;
    size_t OIDLen = conf->_oidLen;

    struct snmp_pdu *pdu, *response;
    struct variable_list *vp;
    int status;
    int64_t ret = 0;

    if (!conf->snmpSessp)
    {
        open(conf);
    }

    pdu = snmp_pdu_create(SNMP_MSG_GET);

    snmp_add_null_var(pdu, OID, OIDLen);

    status = snmp_sess_synch_response(conf->snmpSessp, pdu, &response);

    vp = response->variables;

    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR)
    {
        vp = response->variables;
        if (vp)
        {
            switch (vp->type)
            {
            case ASN_INTEGER:
            case ASN_COUNTER:
            case ASN_GAUGE:
            case ASN_TIMETICKS:
            case ASN_UINTEGER:
                ret = *vp->val.integer;
                break;
            default:
                logger->error("Non-Integer SNMP data received (type={}):", static_cast<int>(vp->type));
                char buf[1024];
                while (vp)
                {
                    snprint_variable(buf, sizeof(buf), vp->name, vp->name_length, vp);
                    logger->error("{}: {}", (&conf->snmpSession)->peername, buf);
                    vp = vp->next_variable;
                }
            }
        }
    }
    else
    {
        std::string errorStr;
        if (status == STAT_SUCCESS)
        {
            errorStr = "Error in packet: " + std::string(snmp_errstring(response->errstat));
        }
        else
        {
            char *err;
            snmp_sess_error(conf->snmpSessp, NULL, NULL, &err);
            errorStr = "SNMP-get: " + std::string(err);
            free(err);
        }

        logger->error("SNMP error: {}", errorStr);
        throw std::runtime_error(errorStr);
    }

    if (response)
    {
        snmp_free_pdu(response);
    }
    return ret;
}

// Sense function with correct signature
void *sensor_sense(void *sensor)
{
    sensor_t *s = (sensor_t *)sensor;
    sensor_attribute_t *conf = s->conf;

    SensorReading *tv = sensor_values[conf->uuid];
    tv->value = get(conf);
    tv->time = plugin_common::getTimestamp();

    // Print the sensor data
    logger->debug("[{}] value={}, timestamp={}", conf->uuid, tv->value, tv->time);
    return tv;
}

void *snmp_run(void *runtime)
{
    return plugin_common::runSensors(
        ::sensors,
        [](sensor_t *sensor)
        {
            const auto *reading =
                static_cast<SensorReading *>(sensor->sense(sensor));
            return plugin_common::makeTimeValueMessage(
                sensor, reading->time, reading->value);
        },
        runtime,
        logger);
}

// Global plugin instance
plugin_interface_t plugin =
    {
        get_version,
        snmp_create,
        snmp_destroy,
        snmp_init_sensors,
        snmp_finit,
        snmp_run,          // run function
        snmp_configurator, // configurator function
};