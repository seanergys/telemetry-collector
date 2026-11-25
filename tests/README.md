<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: EUPL-1.2
-->

# Tests

The [local test workflow](../RUN.md#running-and-testing-locally) shows how to
build the collector, install it, and run all three CTest suites.

| Directory | What it tests | CMake target |
| --- | --- | --- |
| [`plugin_manager/`](plugin_manager/plugin_manager_tests.md) | Plugin loading, lifecycle operations, and status reporting. The suite builds test plugin libraries from `plugin_manager_fixture.cpp`. | `run_plugin_manager_tests` |
| [`plugin_configuration/`](plugin_configuration/plugin_configuration_tests.md) | Shared YAML parsing and the configurators for selected plugins. Input YAML fixtures live under `fixtures/`. | `run_plugin_configuration_tests` |
| [`plugin_tests/`](plugin_tests/plugin_tests.md) | Runs installed plugins against local services. `run.py` starts the services and checks the cases, and `subscribe.cpp` validates published readings. | `run_plugin_tests` |

Each suite defines its CTest cases in `CMakeLists.txt` and runs them through
`run_tests.cmake`. The manager and configuration suites use GoogleTest.
Enable the service-backed plugin tests with `TC_BUILD_PLUGIN_TESTS`.

[Reporting helpers](reporting/) generate the suite reports and the combined
index in the build directory at `test-artifacts/report.md`.

## Historical shell examples

[`main/`](main/) and [`plugins/`](plugins/) contain older shell commands,
sample YAML, and expected output. They are outside CTest and include
paths and endpoints from the original environment. For runnable examples
with local services, use [RUN.md](../RUN.md#run-the-services-manually).
