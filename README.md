<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: EUPL-1.2
-->

# Telemetry Collector

[![GitLab CI and test pipeline status on main](https://gitlab.seanergys.fz-juelich.de/software/telemetry-collector/badges/main/pipeline.svg?key_text=CI%20%26%20tests)](https://gitlab.seanergys.fz-juelich.de/software/telemetry-collector/-/pipelines?ref=main)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)](#source-tree)
[![Documentation](https://img.shields.io/badge/docs-guides-2563EB)](docs/README.md)
[![Configuration reference](https://img.shields.io/badge/configuration-YAML-0F766E)](config/README.md)
[![License EUPL-1.2](https://img.shields.io/badge/license-EUPL--1.2-2563EB)](LICENSE)

[![Build with CMake](https://img.shields.io/badge/build-CMake-064F8C)](INSTALL.md#install-manually-with-cmake)
[![Install with Spack](https://img.shields.io/badge/install-Spack-166534)](INSTALL.md#install-with-spack)
[![Test reports](https://img.shields.io/badge/tests-reports-7C3AED)](#test-reports)
[![Runs on Linux](https://img.shields.io/badge/platform-Linux-334155)](INSTALL.md#prerequisites)
[![Publishes through SeanerBUS](https://img.shields.io/badge/transport-SeanerBUS-0E7490)](RUN.md)

[Install](INSTALL.md) · [Run locally](RUN.md) · [Configuration](config/README.md) · [Plugins](#implemented-plugins) ·
[Test reports](#test-reports) · [Contributing](CONTRIBUTING.md) · [Credits and funding](#credits-and-funding)

**Topics** `telemetry` · `monitoring` · `linux` · `snmp` · `seanerbus` · `cpp` · `cmake` · `spack`

The SEANERGYS Telemetry Collector (TC) is a C++ service that collects timestamped
sensor readings and publishes them to a SeanerBroker. Plugins supply the
readings from sources such as local Linux attributes and network devices.
Subscribers receive numeric values on the SeanerBroker entity identified by
each sensor's UUID.

## Architecture

The TC implements the modular, extensible plugin design described
in SEANERGYS deliverable D2.1. It runs close to the data sources, for example
on compute nodes or facility frontends, and manages the lifecycle of the
telemetry plugins deployed there.

The TC is designed to run continuously as a background service.
On Linux, a service manager such as `systemd` can supervise the process.
The TC keeps its plugins active until it receives a termination
signal such as `SIGTERM` or `SIGINT`, then begins shutting them down.

The TC reads a main YAML configuration with the SeanerBroker URL and a list
of plugin libraries and their configuration files. The plugin manager validates
each shared library's [exported plugin interface](docs/plugin_api.md) and
handles loading, initialization, execution, and cleanup. The plugins use
common code for sensor
configuration, periodic scheduling, and publishing `TimeValueV1` messages.
Where applicable, they use the SeanerBroker naming service to register or resolve
sensor names.

## Source tree

| Directory | Contents |
| --- | --- |
| [`src/`](src/) | TC entry point, YAML configuration reader, plugin manager, and logging implementation. |
| [`include/`](include/) | Plugin interface, runtime and sensor types, and TC headers. |
| [`plugins/`](plugins/) | Plugin implementations and shared helpers in `plugin_common.hpp`. |
| [`config/`](config/README.md) | Main configuration template and example YAML for each implemented plugin. |
| [`docs/`](docs/README.md) | Documentation index and plugin API reference. |
| [`cmake/`](cmake/) | Plugin discovery, dependency resolution, and build support. |
| [`tests/`](tests/README.md) | Plugin manager tests, configuration tests, and plugin tests that run against locally deployed services and componets. |

## Implemented plugins

| Plugin | What it does |
| --- | --- |
| [`sysfs`](plugins/sysfs/) | Reads numeric values from configured Linux paths, such as attributes in `/sys` or files in `/proc`. Supports extracting a value with a filter and applying a factor and offset. |
| [`snmp`](plugins/snmp/) | Polls configured OIDs on SNMP agents and publishes numeric readings. |
| [`ad`](plugins/ad/) | Resolves sensor entities through the SeanerBroker naming service and requests their readings through SeanerBroker. |
| [`tester`](plugins/tester/) | Generates incrementing counter values for testing the publication path without a physical sensor. |
| [`mock`](plugins/mock/) | Provides a minimal plugin for exercising loading, configuration, and lifecycle operations. |

## Installation and usage

See the [configuration reference](config/README.md) for the main YAML and
each plugin’s parameters.

See the [installation guide](INSTALL.md) for the Spack and manual CMake
workflows, and [RUN.md](RUN.md) for running the TC with SeanerBroker.

## Test reports

[Combined CI report](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/report.md?job=test_report)
· [Browse CI artifacts](https://gitlab.seanergys.fz-juelich.de/software/telemetry-collector/-/jobs/artifacts/main/browse?job=test_report)

These reports are from the latest successful pipeline on `main`.

- **[Plugin manager](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-manager/report.md?job=test_report)** · [CTest log](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-manager/ctest.log?job=test_report) · [Manager cases](tests/plugin_manager/plugin_manager_tests.md) · `run_plugin_manager_tests`
- **[Plugin configuration](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-configuration/report.md?job=test_report)** · [CTest log](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-configuration/ctest.log?job=test_report) · [Configuration cases](tests/plugin_configuration/plugin_configuration_tests.md) · `run_plugin_configuration_tests`
- **[Plugin tests](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/report.md?job=test_report)** · [CTest log](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/ctest.log?job=test_report) · [Plugin cases and service setup](tests/plugin_tests/plugin_tests.md) · `run_plugin_tests`

For local runs, open [`build/test-artifacts/report.md`](build/test-artifacts/report.md)
or `test-artifacts/report.md` under your build directory. To investigate a
failed CI run, open the [latest pipeline](https://gitlab.seanergys.fz-juelich.de/software/telemetry-collector/-/pipelines/latest)
and browse the artifacts of its `test_report` job or the individual test jobs.

Each suite report links to its CTest output. With CMake 3.21 or newer, it
also links to JUnit results. With Python 3.6 or newer available at configure
time, the manager and configuration reports link to individual test logs
and available YAML snapshots. The plugin tests report links to the processes
started for each plugin.

## Plugin process logs and YAML configs

| Plugin | Telemetry collector | Naming server | SeanerBroker | SNMP agent | Subscriber | Configs |
| --- | --- | --- | --- | --- | --- | --- |
| Mock | [Collector](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/mock/mock/collector.log?job=test_report) | [Naming](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/mock/naming.log?job=test_report) | [Broker](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/mock/broker.log?job=test_report) | — | — | [Collector YAML](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/mock/mock/telemetry_collector.yaml?job=test_report), [Plugin YAML](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/mock/mock/plugin.yaml?job=test_report) |
| Tester | [Collector](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/tester/tester/collector.log?job=test_report) | [Naming](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/tester/naming.log?job=test_report) | [Broker](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/tester/broker.log?job=test_report) | — | [Subscriber](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/tester/tester/subscriber.log?job=test_report) | [Collector YAML](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/tester/tester/telemetry_collector.yaml?job=test_report), [Plugin YAML](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/tester/tester/plugin.yaml?job=test_report) |
| Sysfs | [Collector](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/sysfs/sysfs/collector.log?job=test_report) | [Naming](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/sysfs/naming.log?job=test_report) | [Broker](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/sysfs/broker.log?job=test_report) | — | [Subscriber](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/sysfs/sysfs/subscriber.log?job=test_report) | [Collector YAML](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/sysfs/sysfs/telemetry_collector.yaml?job=test_report), [Plugin YAML](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/sysfs/sysfs/plugin.yaml?job=test_report) |
| SNMP | [Collector](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/snmp/snmp/collector.log?job=test_report) | [Naming](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/snmp/naming.log?job=test_report) | [Broker](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/snmp/broker.log?job=test_report) | [Agent](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/snmp/snmpd.log?job=test_report) | [Subscriber](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/snmp/snmp/subscriber.log?job=test_report) | [Collector YAML](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/snmp/snmp/telemetry_collector.yaml?job=test_report), [Plugin YAML](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/snmp/snmp/plugin.yaml?job=test_report) |

[Plugin test results (JUnit)](https://gitlab.seanergys.fz-juelich.de/api/v4/projects/software%2Ftelemetry-collector/jobs/artifacts/main/raw/build/test-artifacts/plugin-tests/junit.xml?job=test_report)

## Credits and funding

<a href="https://seanergys.eu/"><img src="docs/assets/seanergys.png" alt="SEANERGYS" width="260"></a>
<a href="https://www.eurohpc-ju.europa.eu/"><img src="docs/assets/eurohpc-ju.png" alt="EuroHPC Joint Undertaking" width="150"></a>

The Telemetry Collector is developed by the partners of
[SEANERGYS (Software for Efficient and Energy-Aware Supercomputers)](https://seanergys.eu/).

The European High Performance Computing Joint Undertaking (EuroHPC JU) funds
SEANERGYS under [grant agreement No. 101177590](https://cordis.europa.eu/project/id/101177590).
The Joint Undertaking receives support from the European Union’s Horizon Europe
research and innovation programme and from the Czech Republic, France, Germany,
Greece, Italy, and Spain. See the [SEANERGYS funding page](https://seanergys.eu/funding/).

The views and opinions are those of the authors. They may differ from those of
the European Union and the granting authority. Neither the European Union nor
the granting authority can be held responsible for them.

