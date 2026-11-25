<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: EUPL-1.2
-->

# Run the collector

## Running and testing locally

Run the commands from the collector checkout. This workflow builds in
`build/` and installs in `install/`. Make the
[manual build dependencies](INSTALL.md#prerequisites), including GoogleTest,
available to CMake.

### 1. Configure, build, and install

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/install" \
  -DUSE_SYSTEM_DEPENDENCIES=ON \
  -DDEPENDENCY_PROVIDER_spdlog=spdlog \
  -DBUILD_TESTING=ON
cmake --build build --parallel 4
cmake --install build
```

The plugin tests use the installed collector and plugins. Reinstall after
changing either.

### 2. Run the manager and configuration tests

```sh
cmake --build build --target \
  run_plugin_manager_tests run_plugin_configuration_tests --parallel 4
```

### 3. Prepare the plugin test services

Keep the same dependency environment for CMake. Have Cargo, Rust, and the
`capnp` compiler on `PATH`, and Git access to the SEANERGYS GitLab repositories
and their submodules.

Build the service revisions used in CI and the subscriber shipped with these
tests:

```sh
(
  set -eu
  tc_services="$PWD/build/run-services"
  mkdir -p "$tc_services"
  git clone git@gitlab.seanergys.fz-juelich.de:software/seanerbroker.git "$tc_services/broker"
  git -C "$tc_services/broker" checkout --detach e7b26f03bb92245555d8369202f3097a600d93bd
  git -C "$tc_services/broker" submodule update --init --recursive
  git clone git@gitlab.seanergys.fz-juelich.de:software/seanerbus-naming-service.git "$tc_services/naming"
  git -C "$tc_services/naming" checkout --detach 9a87240dcec0911be90662d8f2e1a30639649790
  git -C "$tc_services/naming" submodule update --init --recursive
  cargo build --locked --manifest-path "$tc_services/broker/Cargo.toml" --target-dir "$tc_services/target"
  cargo build --locked --manifest-path "$tc_services/naming/Cargo.toml" --target-dir "$tc_services/target" --bin naming_server

  cmake -S . -B build \
    -DTC_BUILD_PLUGIN_TESTS=ON \
    -DTC_PLUGIN_TEST_INSTALL_PREFIX="$PWD/install" \
    -DTC_PLUGIN_TEST_BROKER="$tc_services/target/debug/seanerbroker" \
    -DTC_PLUGIN_TEST_NAMING="$tc_services/target/debug/naming_server" \
    -DTC_PLUGIN_TEST_NAMING_CONFIG="$tc_services/naming/services.json"
  cmake --build build --target test_plugin_subscriber --parallel 4
)
```

The clone commands are for the first setup. Reuse those checkouts on later
runs.

### 4. Run the plugin tests and open the report

Have `snmpd` and `snmpget` on `PATH` for the SNMP test. The fixture starts
and stops its own services, so leave TCP port `5398` and UDP port `1161` free.

```sh
export PATH="/usr/sbin:$PATH"
cmake --build build --target run_plugin_tests --parallel 4
```

Open [`build/test-artifacts/report.md`](build/test-artifacts/report.md) for the
combined report and links to the suite logs. To rerun just the SNMP case:

```sh
ctest --test-dir build --output-on-failure \
  -R '^PluginTest[.]SnmpReadsConfiguredOidAndPublishes$'
```

## Run the services manually

This example runs the tester plugin and receives five readings over SeanerBUS.
After preparing the services and subscriber above, open four terminals at the
collector checkout and start them in order: **t2 → t4 → t3 → t1**. Keep TCP
port 5398 free before starting the broker.

### t2 — SeanerBUS broker

```sh
RUST_LOG=debug ./build/run-services/target/debug/seanerbroker 127.0.0.1:5398
```

### t4 — Naming server

Start this after t2 logs that it is waiting for connections. Run from the
naming server checkout so it can read `services.json`.

```sh
(cd build/run-services/naming && RUST_LOG=debug ../target/debug/naming_server)
```

### t3 — Subscriber

Subscribe to the tester temperature sensor before starting the collector.
The last two arguments are the expected first value and increment. Wait for
the broker to log the subscription before starting t1.

```sh
./build/tests/plugin_tests/test_plugin_subscriber \
  seanerbroker://127.0.0.1:5398 7bc3d210-a49f-4422-8fe8-26ca480d0e1a 1 1
```

### t1 — Telemetry collector

Create a tester-only configuration in the build directory and start the
installed collector.

```sh
(
  set -eu
  tc_prefix="$PWD/install"
  tc_plugin="$(find -H "$tc_prefix" -path '*/telemetry_collector/plugins/tester.so' -print -quit)"
  test -n "$tc_plugin" || { echo "Install the tester plugin first." >&2; exit 1; }
  tc_config="$PWD/build/run-services/telemetry_collector.yaml"
  cat > "$tc_config" <<EOF
seanerbus:
  url: seanerbroker://127.0.0.1:5398
plugin_status_interval_ms: 1000
plugins:
  - path: "$tc_plugin"
    config: "$tc_prefix/etc/telemetry_collector/tester.yaml"
    uuid: e94e6e91-a83d-4e04-a147-aab3860d0cd9
    name: tester
EOF
  "$tc_prefix/bin/telemetry_collector" "$tc_config"
)
```

The subscriber should print values `1` through `5` and exit. Stop the
collector, naming server, and broker with Ctrl+C. If the collector waits
during shutdown, stop the broker so the tester's publishing loop can return.

To run these checks automatically and save the process logs, see the
[plugin tests](tests/plugin_tests/plugin_tests.md).

## Historical instructions

These notes preserve the original dataplane checkout layout, service
commands, environment exports, and configuration overwrite flag.

<details>
<summary>Original t1–t4 run notes</summary>

# Run

## t1

```
cd seanergys/telemetry-collector
 ./../install/bin/telemetry_collector ../install/etc/telemetry_collector/config.yaml

# mod log level
# ../install/etc/telemetry_collector/ -> plugin_name.yaml
```

## t2

```
cd seanergys/dataplane/seanerbus/server
RUST_LOG=debug cargo run
```

## t3

```
cd seanergys/dataplane/seanerbus/cpp_examples
./subscribe_test 6bc3d210-a49f-4422-8fe8-26ca480d0e1f
```

## t4

```
cd seanergys/dataplane/seanerbus/naming_scheme
RUSTFLAGS="-L/home/di93zad/seanergys/dataplane/install_seanerbus/lib64 -lseanerbus"    cargo run --bin naming_server
```

## export

```
export PKG_CONFIG_PATH="$HOME/seanergys/telemetry-collector/install/capnp-0.8/lib/pkgconfig:$PKG_CONFIG_PATH"
export PATH="$HOME/seanergys/telemetry-collector/install/capnp-0.8/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/seanergys/telemetry-collector/install/capnp-0.8/lib:$LD_LIBRARY_PATH"

```

## Vars

### install configs
```
-DTC_CONFIG_OVERWRITE=ON
```

</details>
