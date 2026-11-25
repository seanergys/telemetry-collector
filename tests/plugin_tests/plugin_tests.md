<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: EUPL-1.2
-->

# Plugin tests

Each case launches the installed collector with a configuration containing
one plugin. The [shared Python fixture](run.py) starts a fresh local
SeanerBUS broker and naming server and waits until the broker reports the
naming endpoint registered. The SNMP case also starts its own local agent.
CTest serializes the cases because they use the same loopback ports.

## Cases

The table follows registration order in [CMakeLists.txt](CMakeLists.txt).

| ID | CTest case | Python test | Fixture and behavior under test | Expected result |
| --- | --- | --- | --- | --- |
| 001 | `PluginTest.MockLoadsRunsAndUnloads` | `test_mock_loads_runs_and_unloads` | Load the installed mock library with a minimal plugin config. Wait for its UUID in the collector status report, then send `SIGTERM`. | The status report contains the mock UUID. The collector logs stop and unload for the configured plugin and exits with status `0` within five seconds. |
| 002 | `PluginTest.TesterPublishesFiveCounterReadings` | `test_tester_publishes_five_counter_readings` | Configure one tester sensor and subscribe to its UUID before starting the collector. Exercises the tester's first five counter readings and the registration and publication path. | The naming service registers UUID `7bc3d210-a49f-4422-8fe8-26ca480d0e1a`. The subscriber receives five `TimeValueV1` messages with positive timestamps and values `1` through `5` in order. |
| 003 | `PluginTest.SysfsReadsConfiguredPathAndPublishes` | `test_sysfs_reads_configured_path_and_publishes` | Write `37\n` to a regular fixture file, configure that path as the sensor input, and subscribe before starting the collector. Exercises opening the configured file and parsing the numeric reading through the sysfs plugin. | The naming service registers UUID `7bc3d210-a49f-4422-8fe8-26ca480d0e1b`. The subscriber receives five `TimeValueV1` messages, each with a positive timestamp and value `37`. |
| 004 | `PluginTest.SnmpReadsConfiguredOidAndPublishes` | `test_snmp_reads_configured_oid_and_publishes` | Start `snmpd` on `127.0.0.1:1161` and verify with `snmpget` that OID `.1.3.6.1.4.1.8072.1.3.2.3.1.4.2.116.99` returns the fixture command's exit status, `42`. Configure the plugin for an SNMPv2c GET against that agent and OID. | The naming service registers UUID `7bc3d210-a49f-4422-8fe8-26ca480d0e1c`. The subscriber receives five `TimeValueV1` messages, each with a positive timestamp and value `42`. |

The subscriber exits after validating five published readings. The sysfs case uses a regular
file as input. Sampling processes are cleaned up with a bounded shutdown
after the assertions, and the mock case explicitly asserts clean shutdown.

## Run locally

Start with the [manual CMake build and installation](../../INSTALL.md#install-manually-with-cmake).
You also need Python 3.6 or later, the broker and naming server executables,
and the naming server's `services.json`. The SNMP case needs `snmpd` and
`snmpget` on `PATH`. Keep loopback ports 5398/TCP and 1161/UDP free.

From the collector checkout, enable the cases in your existing build directory:

```sh
cmake -S . -B build \
  -DTC_BUILD_PLUGIN_TESTS=ON \
  -DTC_PLUGIN_TEST_BROKER=/path/to/seanerbroker \
  -DTC_PLUGIN_TEST_NAMING=/path/to/naming_server \
  -DTC_PLUGIN_TEST_NAMING_CONFIG=/path/to/services.json

cmake --build build --target run_plugin_tests --parallel 4
```

The fixture uses the collector and plugins installed under
`CMAKE_INSTALL_PREFIX`. Set `TC_PLUGIN_TEST_INSTALL_PREFIX` if they are
installed elsewhere. Reinstall after changing the collector or a plugin.

After building the subscriber, rerun one case with:

```sh
ctest --test-dir build --output-on-failure -R '^PluginTest[.]SnmpReadsConfiguredOidAndPublishes$'
```

## Logs and artifacts

The artifact directory is `<build-dir>/test-artifacts/plugin-tests/`.
Replace `<plugin>` with `mock`, `tester`, `sysfs`, or `snmp`.
The fixture captures each process's stdout and stderr together.
Open `report.md` in the artifact directory after running the CMake target
to browse the available logs for each plugin.

| Process or artifact | Path relative to the artifact directory |
| --- | --- |
| SeanerBUS broker | `<plugin>/broker.log` |
| Naming server | `<plugin>/naming.log` |
| SNMP agent | `snmp/snmpd.log` |
| Telemetry collector | `<plugin>/<plugin>/collector.log` |
| Subscriber | `<plugin>/<plugin>/subscriber.log` for tester, sysfs, and snmp |
| Collector and plugin configs | `<plugin>/<plugin>/telemetry_collector.yaml` and `<plugin>/<plugin>/plugin.yaml` |
| CTest results | `junit.xml` when running the CMake target with CMake 3.21 or later |

For example, the SNMP subscriber's output is in
`build/test-artifacts/plugin-tests/snmp/snmp/subscriber.log`.
Start with the collector log for plugin startup or shutdown failures, the
naming server log for sensor registration, and the subscriber log for the
received timestamp and value.

The GitLab `plugin_tests` job uploads this directory even on failure. It also
writes `broker-build.log` and `naming-build.log` at the artifact root.
