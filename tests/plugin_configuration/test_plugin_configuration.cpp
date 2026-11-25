#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

void snapshotTestConfig(const std::filesystem::path &filename)
{
    const char *artifactRoot =
        std::getenv("PLUGIN_TEST_ARTIFACT_ROOT");
    const auto *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    if (!artifactRoot || !testInfo || !std::filesystem::exists(filename))
        return;

    std::error_code error;
    const auto destination =
        std::filesystem::path(artifactRoot) /
        (std::string(testInfo->test_suite_name()) + "." + testInfo->name()) /
        "configs" / filename.filename();
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error)
    {
        ADD_FAILURE() << "Could not create test artifact directory: "
                      << error.message();
        return;
    }

    std::filesystem::copy_file(
        filename,
        destination,
        std::filesystem::copy_options::overwrite_existing,
        error);
    if (error)
        ADD_FAILURE() << "Could not save test config '" << filename.string()
                      << "' to '" << destination.string()
                      << "': " << error.message();
}

#if defined(PLUGIN_COMMON_UNIT_TESTS)

#include <uuid/uuid.h>

#include "config.hpp"
#include "plugin.hpp"

struct sensor_attribute : generic_sensor_attribute_t
{
};

#include "plugin_common.hpp"

struct TestSensorConfig : generic_sensor_attribute_t
{
    ~TestSensorConfig()
    {
        delete[] uuid;
        delete[] description;
        delete[] unit;
        delete[] topic;
        delete[] filter;
    }
};

class PluginCommonConfigurationTest : public ::testing::Test
{
protected:
    using ConfigMap = std::map<std::string, TestSensorConfig *>;

    void SetUp() override
    {
        logger = logging.create("plugin-common-configuration-test");
    }

    void TearDown() override
    {
        for (auto &[uuid, config] : configs)
            delete config;
        configs.clear();
        logging.flush();
    }

    YAML::Node loadFixture(const std::string &filename)
    {
        const auto path =
            std::filesystem::path(PLUGIN_COMMON_FIXTURE_DIR) / filename;
        snapshotTestConfig(path);
        return YAML::LoadFile(path.string());
    }

    void *configure(YAML::Node &yaml)
    {
        return plugin_common::commonConfigurationProcedure(
            &yaml,
            logger,
            logging,
            "plugin-common-configuration-test",
            [](const YAML::Node &node, TestSensorConfig *config)
            {
                plugin_common::applyCommonSensorFields(
                    node,
                    config,
                    plugin_common::UuidPolicy::GenerateWhenInvalid);
            },
            configs);
    }

    Logger logging;
    Logger::LoggerPtr logger;
    ConfigMap configs;
};

TEST_F(
    PluginCommonConfigurationTest,
    AppliesTemplateGroupAndSensorOverridesAndGeneratesMissingUuid)
{
    YAML::Node yaml;
    ASSERT_NO_THROW(yaml = loadFixture("configuration_inheritance.yaml"));
    ASSERT_EQ(configure(yaml), &yaml);
    ASSERT_EQ(configs.size(), 2);
    EXPECT_EQ(logger->level(), spdlog::level::debug);

    const auto sensor = configs.find("11111111-1111-4111-8111-111111111111");
    ASSERT_NE(sensor, configs.end());
    EXPECT_STREQ(sensor->second->description, "group description");
    EXPECT_STREQ(sensor->second->unit, "template unit");
    EXPECT_STREQ(sensor->second->topic, "sensor topic");
    EXPECT_STREQ(sensor->second->filter, "sensor filter");
    EXPECT_EQ(sensor->second->interval_ms, std::chrono::milliseconds(250));
    EXPECT_DOUBLE_EQ(sensor->second->factor, 2.5);
    EXPECT_DOUBLE_EQ(sensor->second->offset, -1.5);
    EXPECT_EQ(
        sensor->second->nameserver_parts.at("hostname"), "template host");
    EXPECT_EQ(sensor->second->nameserver_parts.at("rack"), "rack 1");
    EXPECT_EQ(sensor->second->nameserver_parts.at("metric"), "cpu");
    EXPECT_EQ(sensor->second->nameserver_parts.at("shared"), "sensor");

    const auto generated = std::find_if(
        configs.begin(), configs.end(),
        [](const auto &entry)
        { return entry.first != "11111111-1111-4111-8111-111111111111"; });
    ASSERT_NE(generated, configs.end());
    EXPECT_STREQ(generated->second->unit, "percent");
    EXPECT_STREQ(generated->second->topic, "group topic");
    EXPECT_STREQ(generated->second->filter, "group filter");
    EXPECT_EQ(generated->second->interval_ms, std::chrono::milliseconds(2000));
    EXPECT_EQ(generated->second->nameserver_parts.at("metric"), "memory");
    EXPECT_EQ(generated->second->nameserver_parts.at("shared"), "group");

    uuid_t parsedUuid;
    EXPECT_EQ(uuid_parse(generated->first.c_str(), parsedUuid), 0);
    EXPECT_STREQ(generated->second->uuid, generated->first.c_str());
}

TEST_F(PluginCommonConfigurationTest, MissingSensorListLeavesConfigsEmpty)
{
    YAML::Node yaml;
    ASSERT_NO_THROW(yaml = loadFixture("missing_sensors.yaml"));
    ASSERT_EQ(configure(yaml), &yaml);
    EXPECT_TRUE(configs.empty());
}

TEST_F(PluginCommonConfigurationTest, UnknownTemplateStillAppliesSensorFields)
{
    YAML::Node yaml;
    ASSERT_NO_THROW(yaml = loadFixture("unknown_template.yaml"));
    ASSERT_EQ(configure(yaml), &yaml);
    ASSERT_EQ(configs.size(), 1);

    const auto sensor = configs.find("22222222-2222-4222-8222-222222222222");
    ASSERT_NE(sensor, configs.end());
    EXPECT_STREQ(sensor->second->unit, "celsius");
    EXPECT_STREQ(sensor->second->topic, "explicit topic");
}

TEST_F(PluginCommonConfigurationTest, AppliesSensorTemplateBeforeSensorOverrides)
{
    YAML::Node yaml;
    ASSERT_NO_THROW(yaml = loadFixture("sensor_template_overrides.yaml"));
    ASSERT_EQ(configure(yaml), &yaml);
    ASSERT_EQ(configs.size(), 1);

    const auto sensor = configs.find("33333333-3333-4333-8333-333333333333");
    ASSERT_NE(sensor, configs.end());
    EXPECT_EQ(sensor->second->interval_ms, std::chrono::milliseconds(500));
    EXPECT_STREQ(sensor->second->unit, "sensor template unit");
    EXPECT_STREQ(sensor->second->topic, "sensor override topic");
    EXPECT_DOUBLE_EQ(sensor->second->factor, 3.5);
    EXPECT_EQ(sensor->second->nameserver_parts.at("group"), "group value");
    EXPECT_EQ(sensor->second->nameserver_parts.at("shared"), "sensor template");
}

TEST_F(PluginCommonConfigurationTest, MissingSensorGroupsLeavesConfigsEmpty)
{
    YAML::Node yaml;
    ASSERT_NO_THROW(yaml = loadFixture("missing_sensor_groups.yaml"));
    ASSERT_EQ(configure(yaml), &yaml);
    EXPECT_TRUE(configs.empty());
}

TEST_F(PluginCommonConfigurationTest, AppliesUuidOnlyWhenValueIsValid)
{
    TestSensorConfig generated;
    const YAML::Node invalidUuid = YAML::Load("uuid: not-a-uuid");
    plugin_common::applyCommonSensorFields(
        invalidUuid,
        &generated,
        plugin_common::UuidPolicy::GenerateWhenInvalid);
    ASSERT_NE(generated.uuid, nullptr);
    EXPECT_STRNE(generated.uuid, "not-a-uuid");
    uuid_t parsedUuid;
    EXPECT_EQ(uuid_parse(generated.uuid, parsedUuid), 0);

    TestSensorConfig emptyGenerated;
    const YAML::Node emptyUuid = YAML::Load("uuid: \"\"");
    plugin_common::applyCommonSensorFields(
        emptyUuid,
        &emptyGenerated,
        plugin_common::UuidPolicy::GenerateWhenInvalid);
    ASSERT_NE(emptyGenerated.uuid, nullptr);
    EXPECT_EQ(uuid_parse(emptyGenerated.uuid, parsedUuid), 0);

    TestSensorConfig ignored;
    plugin_common::applyCommonSensorFields(
        invalidUuid,
        &ignored,
        plugin_common::UuidPolicy::IgnoreInvalid);
    EXPECT_EQ(ignored.uuid, nullptr);

    TestSensorConfig explicitUuid;
    const YAML::Node validUuid =
        YAML::Load("uuid: 44444444-4444-4444-8444-444444444444");
    plugin_common::applyCommonSensorFields(
        validUuid,
        &explicitUuid,
        plugin_common::UuidPolicy::GenerateWhenInvalid);
    ASSERT_NE(explicitUuid.uuid, nullptr);
    EXPECT_STREQ(explicitUuid.uuid, "44444444-4444-4444-8444-444444444444");
    EXPECT_TRUE(explicitUuid.uuid_specified_in_config);
}

TEST_F(
    PluginCommonConfigurationTest,
    RejectsNameServerUuidThatConflictsWithConfiguredUuid)
{
    TestSensorConfig config;
    const YAML::Node configuredUuid =
        YAML::Load("uuid: 44444444-4444-4444-8444-444444444444");
    plugin_common::applyCommonSensorFields(
        configuredUuid, &config, plugin_common::UuidPolicy::IgnoreInvalid);

    EXPECT_FALSE(plugin_common::applyResolvedUuid(
        config,
        "55555555-5555-4555-8555-555555555555"));
    EXPECT_STREQ(config.uuid, "44444444-4444-4444-8444-444444444444");
    EXPECT_TRUE(plugin_common::applyResolvedUuid(
        config,
        "44444444-4444-4444-8444-444444444444"));
    EXPECT_STREQ(config.uuid, "44444444-4444-4444-8444-444444444444");
}

TEST_F(PluginCommonConfigurationTest, GeneratesDistinctUuidsForMissingSensorUuids)
{
    YAML::Node yaml;
    ASSERT_NO_THROW(yaml = loadFixture("missing_sensor_uuids.yaml"));
    ASSERT_EQ(configure(yaml), &yaml);
    ASSERT_EQ(configs.size(), 2);

    auto sensor = configs.begin();
    ASSERT_NE(sensor->second->uuid, nullptr);
    uuid_t parsedUuid;
    EXPECT_EQ(uuid_parse(sensor->first.c_str(), parsedUuid), 0);
    EXPECT_STREQ(sensor->first.c_str(), sensor->second->uuid);

    ++sensor;
    ASSERT_NE(sensor, configs.end());
    ASSERT_NE(sensor->second->uuid, nullptr);
    EXPECT_EQ(uuid_parse(sensor->first.c_str(), parsedUuid), 0);
    EXPECT_STREQ(sensor->first.c_str(), sensor->second->uuid);
    EXPECT_NE(configs.begin()->first, sensor->first);
}

TEST_F(PluginCommonConfigurationTest, AppliesLogLevelCaseInsensitively)
{
    YAML::Node yaml = YAML::Load(
        "log_level: WARN\n"
        "sensor_groups:\n"
        "  example:\n"
        "    sensors:\n"
        "      - uuid: 55555555-5555-4555-8555-555555555555\n");

    ASSERT_EQ(configure(yaml), &yaml);
    EXPECT_EQ(logger->level(), spdlog::level::warn);
    EXPECT_EQ(configs.size(), 1);
}

TEST_F(PluginCommonConfigurationTest, InvalidLogLevelKeepsExistingLevel)
{
    const auto originalLevel = logger->level();
    YAML::Node yaml = YAML::Load(
        "log_level: not-a-level\n"
        "sensor_groups:\n"
        "  example:\n"
        "    sensors: []\n");

    ASSERT_EQ(configure(yaml), &yaml);
    EXPECT_EQ(logger->level(), originalLevel);
}

TEST_F(PluginCommonConfigurationTest, InvalidIntervalTypeThrowsYamlConversionError)
{
    TestSensorConfig config;
    const YAML::Node invalidInterval = YAML::Load("interval_ms: not-a-number");
    EXPECT_THROW(
        plugin_common::applyCommonSensorFields(
            invalidInterval,
            &config,
            plugin_common::UuidPolicy::GenerateWhenInvalid),
        YAML::BadConversion);
}

TEST_F(PluginCommonConfigurationTest, EmptySensorListLeavesConfigsEmpty)
{
    YAML::Node yaml = YAML::Load(
        "sensor_groups:\n"
        "  empty_group:\n"
        "    sensors: []\n");

    ASSERT_EQ(configure(yaml), &yaml);
    EXPECT_TRUE(configs.empty());
}

TEST_F(
    PluginCommonConfigurationTest,
    MainConfigEnablesPluginStatusLogging)
{
    const auto telemetryCollectorConfig =
        load_tc_config(TELEMETRY_COLLECTOR_MAIN_CONFIG, logger);

    ASSERT_TRUE(telemetryCollectorConfig.has_value());
    EXPECT_EQ(
        telemetryCollectorConfig->plugin_status_interval,
        std::chrono::milliseconds(1000));
    ASSERT_FALSE(telemetryCollectorConfig->plugins.empty());
    EXPECT_STREQ(telemetryCollectorConfig->plugins.front().name, "mock");

    for (auto &pluginConfig : telemetryCollectorConfig->plugins)
    {
        std::free(pluginConfig.path);
        std::free(pluginConfig.uuid);
        std::free(pluginConfig.config);
        std::free(pluginConfig.name);
    }
}

#else

#include <dlfcn.h>

#include "plugin.hpp"

#if defined(PLUGIN_CONFIG_SNMP)
#define configs smoke_configs
#define sensors smoke_sensors
#include "snmp_plugin.hpp"
#undef sensors
#undef configs
#elif defined(PLUGIN_CONFIG_SYSFS)
#define configs smoke_configs
#define sensors smoke_sensors
#include "sysfs_plugin.hpp"
#undef sensors
#undef configs
#elif defined(PLUGIN_CONFIG_TESTER)
#define configs smoke_configs
#define sensors smoke_sensors
#include "tester_plugin.hpp"
#undef sensors
#undef configs
#endif

#if defined(PLUGIN_CONFIG_MOCK)
#define SHIPPED_CONFIG_TEST_SUITE MockPluginConfigurationTest
#elif defined(PLUGIN_CONFIG_SNMP)
#define SHIPPED_CONFIG_TEST_SUITE SnmpPluginConfigurationTest
#elif defined(PLUGIN_CONFIG_SYSFS)
#define SHIPPED_CONFIG_TEST_SUITE SysfsPluginConfigurationTest
#elif defined(PLUGIN_CONFIG_TESTER)
#define SHIPPED_CONFIG_TEST_SUITE TesterPluginConfigurationTest
#endif

struct LoadedPlugin
{
    void *library = nullptr;
    plugin_interface_t *plugin = nullptr;
};

LoadedPlugin loadTestPlugin()
{
    LoadedPlugin loaded;
    loaded.library = dlopen(PLUGIN_LIBRARY_PATH, RTLD_NOW | RTLD_LOCAL);
    if (!loaded.library)
    {
        ADD_FAILURE() << dlerror();
        return loaded;
    }

    loaded.plugin =
        static_cast<plugin_interface_t *>(dlsym(loaded.library, "plugin"));
    if (!loaded.plugin)
    {
        ADD_FAILURE() << dlerror();
        return loaded;
    }
    if (!loaded.plugin->create || !loaded.plugin->configurator)
    {
        ADD_FAILURE() << "Plugin is missing its create or configurator function";
        loaded.plugin = nullptr;
        return loaded;
    }
    if (!loaded.plugin->create())
    {
        ADD_FAILURE() << "Plugin create function returned null";
        loaded.plugin = nullptr;
    }
    return loaded;
}

#if !defined(PLUGIN_CONFIG_MOCK)
using ConfigMap = decltype(smoke_configs);

ConfigMap *getSensorConfigMap(void *library)
{
    // The exported C++ global uses this Itanium ABI symbol on Linux.
    return static_cast<ConfigMap *>(dlsym(library, "_Z7configsB5cxx11"));
}
#endif

TEST(SHIPPED_CONFIG_TEST_SUITE, ParsesYamlThroughConfigurator)
{
    ASSERT_TRUE(std::filesystem::exists(PLUGIN_CONFIG_FILE_PATH));
    ASSERT_TRUE(std::filesystem::exists(PLUGIN_LIBRARY_PATH));
    snapshotTestConfig(PLUGIN_CONFIG_FILE_PATH);

    const auto loaded = loadTestPlugin();
    auto *plugin = loaded.plugin;
    ASSERT_NE(plugin, nullptr);

    YAML::Node config;
    ASSERT_NO_THROW(config = YAML::LoadFile(PLUGIN_CONFIG_FILE_PATH));
    config["seanerbus"]["url"] = "seanerbroker://127.0.0.1:5398";

    void *configured = nullptr;
    ASSERT_NO_THROW(configured = plugin->configurator(&config));
    EXPECT_EQ(configured, &config);

#if !defined(PLUGIN_CONFIG_MOCK)
    auto *sensorConfigs = getSensorConfigMap(loaded.library);
    ASSERT_NE(sensorConfigs, nullptr);
    ASSERT_FALSE(sensorConfigs->empty());

    for (const auto &[uuid, sensor] : *sensorConfigs)
    {
        ASSERT_NE(sensor, nullptr);
        ASSERT_NE(sensor->uuid, nullptr);
        EXPECT_EQ(uuid, sensor->uuid);
    }

#if defined(PLUGIN_CONFIG_SYSFS)
    const auto sensor = sensorConfigs->find("4bc3d210-a49f-4422-8fe8-26ca480d0e1f");
    ASSERT_NE(sensor, sensorConfigs->end());
    ASSERT_NE(sensor->second->path, nullptr);
    EXPECT_STREQ(sensor->second->path, "/proc/loadavg");
    EXPECT_TRUE(std::filesystem::exists(sensor->second->path));
    EXPECT_TRUE(sensor->second->uuid_specified_in_config);
#elif defined(PLUGIN_CONFIG_SNMP)
    const auto sensor = sensorConfigs->find("41c3d210-a49f-4422-8fe8-26ca480d0e1f");
    ASSERT_NE(sensor, sensorConfigs->end());
    EXPECT_EQ(sensor->second->oid_suffix, ".1");
#elif defined(PLUGIN_CONFIG_TESTER)
    const auto sensor = sensorConfigs->find("7bc3d210-a49f-4422-8fe8-26ca480d0e1a");
    ASSERT_NE(sensor, sensorConfigs->end());
    EXPECT_STREQ(sensor->second->unit, "C");
#endif
#endif
}

#if defined(PLUGIN_CONFIG_SYSFS)
TEST(
    SysfsPluginConfigurationTest,
    SkipsSensorsWithMissingOrNonexistentPath)
{
    const auto loaded = loadTestPlugin();
    ASSERT_NE(loaded.plugin, nullptr);
    auto *sensorConfigs = getSensorConfigMap(loaded.library);
    ASSERT_NE(sensorConfigs, nullptr);

    const auto missingPath =
        std::filesystem::temp_directory_path() /
        ("telemetry-collector-missing-sysfs-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_FALSE(std::filesystem::exists(missingPath));

    YAML::Node config = YAML::Load(
        "sensor_groups:\n"
        "  valid:\n"
        "    path: /proc/loadavg\n"
        "    sensors:\n"
        "      - uuid: 99999999-9999-4999-8999-999999999999\n"
        "  missing_file:\n"
        "    path: " + missingPath.string() + "\n"
        "    sensors:\n"
        "      - uuid: a9999999-9999-4999-8999-999999999999\n"
        "  missing_path:\n"
        "    sensors:\n"
        "      - uuid: b9999999-9999-4999-8999-999999999999\n");

    ASSERT_EQ(loaded.plugin->configurator(&config), &config);
    ASSERT_EQ(sensorConfigs->size(), 1);
    EXPECT_NE(
        sensorConfigs->find("99999999-9999-4999-8999-999999999999"),
        sensorConfigs->end());
    EXPECT_EQ(
        sensorConfigs->count("a9999999-9999-4999-8999-999999999999"), 0);
    EXPECT_EQ(
        sensorConfigs->count("b9999999-9999-4999-8999-999999999999"), 0);
    EXPECT_FALSE(std::filesystem::exists(missingPath));
}
#elif defined(PLUGIN_CONFIG_SNMP)
TEST(
    SnmpPluginConfigurationTest,
    SkipsSensorsMissingRequiredOidOrConnection)
{
    const auto loaded = loadTestPlugin();
    ASSERT_NE(loaded.plugin, nullptr);
    auto *sensorConfigs = getSensorConfigMap(loaded.library);
    ASSERT_NE(sensorConfigs, nullptr);

    YAML::Node config = YAML::Load(
        "sensor_groups:\n"
        "  missing_oid:\n"
        "    sensors:\n"
        "      - uuid: 88888888-8888-4888-8888-888888888888\n"
        "        connection: 127.0.0.1:161\n"
        "        version: 2\n"
        "  missing_connection:\n"
        "    sensors:\n"
        "      - uuid: 99999999-9999-4999-8999-999999999999\n"
        "        oid_prefix: .1.3.6.1.2.1\n"
        "        oid_suffix: .1.1.0\n"
        "        version: 2\n"
        "  valid:\n"
        "    sensors:\n"
        "      - uuid: a9999999-9999-4999-8999-999999999999\n"
        "        connection: 127.0.0.1:1\n"
        "        oid_prefix: .1.3.6.1.2.1\n"
        "        oid_suffix: .1.1.0\n"
        "        version: 2\n");

    ASSERT_EQ(loaded.plugin->configurator(&config), &config);
    ASSERT_EQ(sensorConfigs->size(), 1);
    EXPECT_EQ(
        sensorConfigs->count("88888888-8888-4888-8888-888888888888"), 0);
    EXPECT_EQ(
        sensorConfigs->count("99999999-9999-4999-8999-999999999999"), 0);
    EXPECT_EQ(
        sensorConfigs->count("a9999999-9999-4999-8999-999999999999"), 1);
}
#endif

#endif

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
