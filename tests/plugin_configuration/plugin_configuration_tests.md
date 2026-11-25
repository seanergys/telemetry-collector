<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: EUPL-1.2
-->

# Plugin Configuration Tests

These tests form the `plugin_configuration` group. The cases below follow
their registration order in
[`CMakeLists.txt`](./CMakeLists.txt) and match the test implementation in
[`test_plugin_configuration.cpp`](./test_plugin_configuration.cpp).

## Shared configuration helper

The first three cases exercise shared parsing in
[`plugin_common.hpp`](../../plugins/plugin_common.hpp) with hand-authored
fixtures. They use a test sensor config type to verify the common
configuration procedure and field-merging behavior without testing the
contents of the shipped plugin YAML files.

| Test ID | Test | Fixture | Scenario and asserted result |
|---------|------|---------|------------------------------|
| 001 | `PluginCommonConfigurationTest.AppliesTemplateGroupAndSensorOverridesAndGeneratesMissingUuid` | [`configuration_inheritance.yaml`](./fixtures/plugin_common/configuration_inheritance.yaml) | Applies template → group → sensor fields and checks precedence, nameserver-part merging, logger level, and the two resulting UUID-keyed configs. The second sensor omits UUID; a valid UUID is generated for its in-memory config. |
| 002 | `PluginCommonConfigurationTest.MissingSensorListLeavesConfigsEmpty` | [`missing_sensors.yaml`](./fixtures/plugin_common/missing_sensors.yaml) | Configures a group without a `sensors` key. The procedure returns the input YAML node and leaves the config map empty. |
| 003 | `PluginCommonConfigurationTest.UnknownTemplateStillAppliesSensorFields` | [`unknown_template.yaml`](./fixtures/plugin_common/unknown_template.yaml) | Refers to an unavailable template. The procedure returns the input YAML node and still creates the sensor config with its explicit UUID, unit, and topic. |
| 004 | `PluginCommonConfigurationTest.AppliesSensorTemplateBeforeSensorOverrides` | [`sensor_template_overrides.yaml`](./fixtures/plugin_common/sensor_template_overrides.yaml) | Checks a sensor-level template is applied after group fields, then the sensor's explicit topic overrides the template. Confirms the template's interval, unit, and factor survive, and nameserver parts merge with the sensor template overriding the shared key. |
| 005 | `PluginCommonConfigurationTest.MissingSensorGroupsLeavesConfigsEmpty` | [`missing_sensor_groups.yaml`](./fixtures/plugin_common/missing_sensor_groups.yaml) | Omits `sensor_groups`; verifies the procedure returns the original YAML node and creates no configs. The implementation logs the missing key. |
| 006 | `PluginCommonConfigurationTest.AppliesUuidOnlyWhenValueIsValid` | In-memory YAML nodes | A valid explicit UUID is preserved and marked as specified in the shared sensor metadata. A malformed UUID is replaced with a generated valid UUID under `GenerateWhenInvalid`; an empty UUID also generates a valid UUID. Under `IgnoreInvalid`, a malformed UUID leaves the field unset. |
| 007 | `PluginCommonConfigurationTest.GeneratesDistinctUuidsForMissingSensorUuids` | [`missing_sensor_uuids.yaml`](./fixtures/plugin_common/missing_sensor_uuids.yaml) | Two sensors omit UUID; verifies each receives a distinct, valid UUID in its runtime config and each map key matches that config UUID. |
| 008 | `PluginCommonConfigurationTest.AppliesLogLevelCaseInsensitively` | In-memory YAML node | Configures `WARN`; confirms uppercase log-level spelling sets the logger to warning and the sensor config is still produced. |
| 009 | `PluginCommonConfigurationTest.InvalidLogLevelKeepsExistingLevel` | In-memory YAML node | Configures an unknown log-level string; verifies configuration continues and the logger retains its previous level. |
| 010 | `PluginCommonConfigurationTest.InvalidIntervalTypeThrowsYamlConversionError` | In-memory YAML node | Supplies a non-numeric `interval_ms`; applying common fields throws `YAML::BadConversion` rather than silently accepting it. |
| 011 | `PluginCommonConfigurationTest.EmptySensorListLeavesConfigsEmpty` | In-memory YAML node | Supplies `sensors: []`; verifies an empty sensor list is accepted and creates no configs. |
| 012 | `PluginCommonConfigurationTest.MainConfigEnablesPluginStatusLogging` | Generated main config | Loads the generated main config through the collector config parser and verifies it enables periodic status logging at 1000 ms with the mock plugin configured. |

## Shipped plugin configurations

When a plugin is selected in CMake, its corresponding shipped-config case is
added. Cases 013–018 below describe the four plugin targets enabled in this
project's default and CI configurations. Each case passes its shipped YAML
file from [`config/`](../../config/) through the plugin's configurator, then
asserts the resulting runtime config values. The tests do not separately
validate YAML text or schema. They also check that the config file and built
plugin library exist, the exported plugin interface exposes its creation and
configuration functions, and the configurator returns the input YAML node.

| Test ID | CTest registration | GoogleTest case | Configuration | Scenario and asserted result |
|---------|--------------------|-----------------|---------------|------------------------------|
| 013 | `PluginConfiguration_mock` | `MockPluginConfigurationTest.ParsesYamlThroughConfigurator` | [`mock.yaml`](../../config/mock.yaml) | Loads the mock plugin and invokes its passthrough configurator. Verifies the returned pointer is the input node. The mock plugin does not expose a sensor-config map. |
| 014 | `PluginConfiguration_snmp` | `SnmpPluginConfigurationTest.ParsesYamlThroughConfigurator` | [`snmp.yaml`](../../config/snmp.yaml) | Runs the shipped file through the SNMP configurator, verifies the UUID-keyed sensor map is populated, and checks that the first sensor's OID suffix is applied to its runtime config. |
| 015 | `PluginConfiguration_snmp_missing_required_fields` | `SnmpPluginConfigurationTest.SkipsSensorsMissingRequiredOidOrConnection` | In-memory config using the SNMP sensor schema | Provides sensors missing an OID and a connection, alongside one valid sensor. Verifies the configurator completes, drops the invalid sensors, and keeps the valid one. The configurator logs a warning for each missing required field. |
| 016 | `PluginConfiguration_sysfs` | `SysfsPluginConfigurationTest.ParsesYamlThroughConfigurator` | [`sysfs.yaml`](../../config/sysfs.yaml) | Runs the shipped file through the sysfs configurator and verifies the UUID-keyed sensor map is populated. Checks that one configured path is `/proc/loadavg` and exists on the test host. |
| 017 | `PluginConfiguration_sysfs_missing_path` | `SysfsPluginConfigurationTest.SkipsSensorsWithMissingOrNonexistentPath` | In-memory config using the sysfs sensor schema | Provides a valid sensor alongside one with a unique nonexistent path and one without a path. Verifies configuration warns and skips the invalid sensors while retaining the valid sensor. |
| 018 | `PluginConfiguration_tester` | `TesterPluginConfigurationTest.ParsesYamlThroughConfigurator` | [`tester.yaml`](../../config/tester.yaml) | Runs the shipped file through the tester configurator, verifies the UUID-keyed map is populated, and checks that one sensor's configured unit is applied. |

The test substitutes a loopback Seanerbus URL before invoking each
configurator; it does not attempt message delivery or establish a broker
connection. AD is excluded because its configurator needs Seanerbus name
resolution.

## Running and artifacts

Configure only this group with:

```sh
cmake -S . -B build-configuration \
  -DTC_BUILD_PLUGIN_MANAGER_TESTS=OFF \
  -DTC_BUILD_PLUGIN_CONFIGURATION_TESTS=ON
cmake --build build-configuration --target run_plugin_configuration_tests --parallel 4
```

Its JUnit report is
`build-configuration/test-artifacts/plugin-configuration/junit.xml`.
Input YAML snapshots are stored per test under
`build-configuration/test-artifacts/plugin-configuration/<test-suite>.<test-name>/configs/`.

Open `test-artifacts/plugin-configuration/report.md` in the build directory for the suite log, JUnit results, and available per-case logs and config snapshots.
