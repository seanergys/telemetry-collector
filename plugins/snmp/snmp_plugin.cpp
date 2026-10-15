#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <map>
#include <algorithm>
#include <queue>
#include <yaml-cpp/yaml.h>
#include <string>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/mib_api.h>

#include "snmp_plugin.hpp"

#include <seanerbus/client/connection.hpp>
#include "seanerbus/client/simple_event_loop.hpp"
#include <seanerbus/helpers/resolve.hpp>

#include <uuid/uuid.h>

static std::map<std::string, SensorReading *> sensor_values;
static Logger logging;
static std::shared_ptr<spdlog::logger> logger;

// Global plugin instance
plugin_interface_t plugin =
    {
        1, // version
        snmp_create,
        snmp_destroy,
        snmp_init_sensors,
        nullptr, // init_actors
        snmp_finit,
        snmp_initlib,
        nullptr,
        snmp_run,          // run function
        nullptr,           // req
        nullptr,           // res
        snmp_configurator, // configurator function
        snmp_free_config,  // free_config function
};

void *sensor_sense(void *sensor);

void *snmp_create(void)
{
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

static void setCharField(char *&field, const std::string &value)
{
    delete[] field;
    field = new char[value.size() + 1];
    std::memcpy(field, value.c_str(), value.size() + 1);
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
    if (!node)
        return;

    if (node["uuid"])
    {

        if (node["uuid"].IsScalar() && !node["uuid"].as<std::string>().empty())
        {
            setCharField(conf->uuid, node["uuid"].as<std::string>());
        }
        else
        {
            uuid_t id;
            uuid_generate_random(id); // V4

            char buf[37];
            uuid_unparse_lower(id, buf);
            std::string uuid(buf);

            setCharField(conf->uuid, uuid);
        }
    }

    if (node["topic"])
        setCharField(conf->topic, node["topic"].as<std::string>());
    if (node["interval_ms"])
        conf->interval_ms = std::chrono::milliseconds(node["interval_ms"].as<uint64_t>());
    if (node["description"])
        setCharField(conf->description, node["description"].as<std::string>());
    if (node["unit"])
        setCharField(conf->unit, node["unit"].as<std::string>());
    if (node["filter"])
        setCharField(conf->filter, node["filter"].as<std::string>());
    if (node["factor"])
        conf->factor = node["factor"].as<double>();
    if (node["offset"])
        conf->offset = node["offset"].as<double>();
    if (node["connection"])
        setCharField(conf->connection, node["connection"].as<std::string>());
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
        setCharField(conf->type, node["type"].as<std::string>());
    if (node["oid_prefix"])
        conf->oid_prefix = node["oid_prefix"].as<std::string>();
    if (node["oid_suffix"])
        conf->oid_suffix = node["oid_suffix"].as<std::string>();

    if (node["nameserver_parts"])
    {
        for (const auto &p : node["nameserver_parts"])
        {
            std::string k = p.first.as<std::string>();
            ;
            std::string v = p.second.as<std::string>();
            ;
            conf->nameserver_parts[k] = v;
        }
    }
}

bool applyTemplateConfigs(const YAML::Node &node, const YAML::Node &templates, sensor_attribute_t *conf)
{
    if (!node["template"] || !templates)
    {
        return false;
    }

    std::string tmplName = node["template"].as<std::string>();
    YAML::Node tmpl = templates[tmplName];

    if (!tmpl)
    {
        logger->warn("Unknown template referenced: '{}'", tmplName);
        return false;
    }

    applyFields(tmpl, conf);
    return true;
}

void setOID(sensor_attribute_t *conf)
{
    const std::string tmp_oid = conf->oid_prefix + conf->oid_suffix;

    // std::cerr << conf->oid_prefix  << " " << conf->oid_suffix << std::endl;

    size_t newoidnamelen = MAX_OID_LEN;
    if (read_objid(tmp_oid.c_str(), conf->_oid, &newoidnamelen))
    {
        conf->_oidLen = newoidnamelen;
    }
    else
    {
        conf->_oidLen = 0;
        logger->error("Cannot convert OID string: {}", tmp_oid);
    }
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

    // init_snmp("SNMPplugin");

    // Get the plugin config from runtime

    // Connect to the server.
    seanerbus::client::Connection conn("demobus://localhost:5398", "demobus://localhost:5398");

    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));

    for (const auto &[uuid, conf] : configs)
    {
        sensor_t *sensor = new sensor_t();
        setOID(conf);

        setup_snmp_session(conf);

        // Set the sensor configuration and sense function
        sensor->conf = conf;
        sensor->sense = sensor_sense;

        // Add to the global sensors map using UUID as key
        ::sensors[uuid] = sensor;
        sensor_values[conf->uuid] = new SensorReading{0, 0.0};

        auto entity = seanerbus::msg::Entity{seanerbus::msg::Uuid{conf->uuid}};

        // appending hostname to the name []
        auto it = conf->nameserver_parts.find("hostname");
        const bool has_hostname = it != conf->nameserver_parts.end() && !it->second.empty();

        if (!has_hostname)
        {
            conf->nameserver_parts["hostname"] = hostname;
        }

        // generating name str
        std::string nameserver_parts_str;
        for (const auto &[k, v] : conf->nameserver_parts)
        {
            nameserver_parts_str += k + "=" + v + ",";
        }
        nameserver_parts_str.pop_back();
        logger->info("Registering sensor {{}}: {}", nameserver_parts_str, conf->uuid);

        seanerbus::resolve::register_resolve(conn, conf->nameserver_parts, entity,
                                             [](seanerbus::client::Connection &conn,
                                                seanerbus::Expected<void, seanerbus::Error> res)
                                             {
                                                 if (!res.ok())
                                                 {
                                                     conn.shutdown();
                                                     return;
                                                 }
                                                 conn.shutdown();
                                             });
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

uint64_t getTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// Sense function with correct signature
void *sensor_sense(void *sensor)
{
    sensor_t *s = (sensor_t *)sensor;
    sensor_attribute_t *conf = s->conf;

    SensorReading *tv = sensor_values[conf->uuid];
    tv->value = get(conf);
    tv->time = getTimestamp();

    // Print the sensor data
    logger->debug("[{}] value={}, timestamp={}", conf->uuid, tv->value, tv->time);
    return tv;
}

struct ScheduledSensor
{
    std::chrono::steady_clock::time_point next_run;
    sensor_t *sensor;
};
struct CompareNextRun
{
    bool operator()(const ScheduledSensor &a,
                    const ScheduledSensor &b) const
    {
        return a.next_run > b.next_run;
    }
};

void read_publish_schedule_next(seanerbus::client::Connection &conn,
                                std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue);

void on_publish(seanerbus::client::Connection &conn,
                std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue,
                seanerbus::Expected<void, seanerbus::Error> err)
{
    if (!err.ok())
    {
        logger->error("Publishing failed: {}", err.unwrap_error().what());
        conn.shutdown(); // error path only — matches the base example
        return;
    }
    read_publish_schedule_next(conn, queue);
}

void read_publish_schedule_next(seanerbus::client::Connection &conn,
                                std::priority_queue<ScheduledSensor, std::vector<ScheduledSensor>, CompareNextRun> &queue)
{
    auto s = queue.top();
    queue.pop();

    auto now = std::chrono::steady_clock::now();
    if (s.next_run > now)
    {
        std::this_thread::sleep_until(s.next_run);
    }

    SensorReading *tv = static_cast<SensorReading *>(s.sensor->sense(s.sensor));

    seanerbus::msg::MessageBuilder<TimeValueV1> msg_builder;
    msg_builder.builder().setTime(tv->time);
    msg_builder.builder().setValue(tv->value);

    seanerbus::msg::Entity entity =
        seanerbus::msg::Entity{seanerbus::msg::Uuid{s.sensor->conf->uuid}};

    s.next_run += s.sensor->conf->interval_ms;
    queue.push(s);

    conn.publish(msg_builder.to_message(entity),
                 [&conn, &queue](seanerbus::client::Connection &c,
                                 seanerbus::Expected<void, seanerbus::Error> e)
                 {
                     on_publish(c, queue, e);
                 });
}
void *snmp_run(void *runtime)
{

    std::priority_queue<
        ScheduledSensor,
        std::vector<ScheduledSensor>,
        CompareNextRun>
        queue;

    for (auto &s : ::sensors)
    {
        queue.push({std::chrono::steady_clock::now(),
                    s.second});
    }

    seanerbus::client::Connection conn("demobus://localhost:5398", "demobus://localhost:5398");
    read_publish_schedule_next(conn, queue);

    seanerbus::client::SimpleEventLoop sl(conn);
    sl.run();

    if (!conn.result().ok())
    {
        logger->error("publish_test failed: {}", conn.result().get_error().what());
        // return -1;
    }
}

void *snmp_configurator(void *configPtr)
{
    // This function receives the YAML object from the plugin manager
    // and returns a plugin-specific config object

    // Cast the void* back to YAML::Node*
    YAML::Node *yaml = static_cast<YAML::Node *>(configPtr);
    YAML::Node &config = *yaml;

    const YAML::Node &templates = config["sensor_templates"];
    if (!templates)
    {
        logger->info("No sensor_templates");
    }

    if (!config["sensor_groups"])
    {
        logger->error("Missing sensor_groups");
        return yaml;
    }

    // Load sensor groups from the YAML config
    for (const auto &groupEntry : config["sensor_groups"])
    {

        std::string groupName = groupEntry.first.as<std::string>();
        const YAML::Node &group = groupEntry.second;

        // sensor list check
        if (!group["sensors"])
        {
            logger->error("Group '{}' missing sensors list", groupName);
            continue;
        }

        for (const auto &item : group["sensors"])
        {

            // Create a sensor_attribute_t for this sensor
            sensor_attribute_t *conf = new sensor_attribute_t();

            // apply group-level template attr
            if (templates)
            {
                applyTemplateConfigs(group, templates, conf);
            }

            // apply group-level config attr
            applyFields(group, conf);

            // apply sensor-level config attr
            applyFields(item, conf);

            // Add to the global sensors map using UUID as key
            ::configs[conf->uuid] = conf;
        }
    }

    // Since the configurator returns a YAML::Node*, we need to delete it

    return yaml;
}

bool snmp_free_config(void *config)
{
    if (!config)
    {
        return false;
    }
    return true;
}
