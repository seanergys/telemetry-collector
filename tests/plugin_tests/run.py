#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: EUPL-1.2

import argparse
from contextlib import ExitStack, contextmanager
import json
from pathlib import Path
import re
import subprocess
import time
from typing import NamedTuple


BROKER = "127.0.0.1:5398"
ENDPOINT = f"seanerbroker://{BROKER}"
SNMP_AGENT = "127.0.0.1:1161"
# Net-SNMP exposes the exit status of the "tc" extend command at this OID.
SNMP_OID = ".1.3.6.1.4.1.8072.1.3.2.3.1.4.2.116.99"
SNMP_COMMUNITY = "tc-test"
SYSFS_READING = 37
SNMP_READING = 42
PLUGIN_UUID = "e94e6e91-a83d-4e04-a147-aab3860d0cd0"
NAMING_SERVICE_UUID = "00000000-0000-0000-0000-0000010ca105"

class PluginTestFixture(NamedTuple):
    install_prefix: Path
    subscriber: Path
    broker_log: Path
    naming_log: Path
    services: list


@contextmanager
def run_process(command, log, working_directory):
    with log.open("w") as output:
        process = subprocess.Popen(command, cwd=working_directory, stdout=output, stderr=subprocess.STDOUT)
        try:
            yield process
        except (RuntimeError, OSError, subprocess.TimeoutExpired):
            lines = log.read_text(errors="replace").splitlines()
            if lines:
                print(f"Last output from {log}:", flush=True)
                print("\n".join(lines[-15:]), flush=True)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    # Sampling plugins may still be blocked in the broker event loop.
                    process.kill()
                    process.wait()
                    print(f"Stopped {log.parent.name}/{log.stem} after its shutdown deadline", flush=True)


def wait_for(description, condition, *processes):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        for process in processes:
            if process.poll() is not None:
                raise RuntimeError(f"{process.args[0]} exited with {process.returncode} before {description}")
        if condition():
            return
        time.sleep(0.05)
    raise RuntimeError(f"Timed out waiting for {description}")


def write_config(path, contents):
    # JSON maps and sequences are accepted by the collector's YAML parser.
    path.write_text(json.dumps(contents, indent=2) + "\n")
    return path


def start_snmp_agent(processes, directory, broker, naming_server):
    # The SNMP plugin performs a real GET. The agent returns this local script's
    # exit status as an integer at SNMP_OID.
    fixture = directory / "snmp-value.sh"
    fixture.write_text(f"#!/bin/sh\nexit {SNMP_READING}\n")
    fixture.chmod(0o755)

    config = directory / "snmpd.conf"
    config.write_text(
        f"agentaddress udp:{SNMP_AGENT}\n"
        f"rocommunity {SNMP_COMMUNITY} 127.0.0.1\n"
        f"extend tc {fixture}\n"
    )
    agent = processes.enter_context(run_process(
        ["snmpd", "-f", "-Lo", "-C", "-c", config], directory / "snmpd.log", directory
    ))

    def fixture_is_ready():
        reply = subprocess.run(
            ["snmpget", "-v2c", "-c", SNMP_COMMUNITY, "-Oqv", "-t", "0.2", "-r", "0",
             SNMP_AGENT, SNMP_OID],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, timeout=2,
        )
        return reply.returncode == 0 and reply.stdout.strip() == str(SNMP_READING)

    wait_for(f"SNMP fixture to return {SNMP_READING}", fixture_is_ready, agent, broker, naming_server)
    return agent


def start_services(processes, args, plugin_names):
    directory = args.logs
    broker_log = directory / "broker.log"
    naming_log = directory / "naming.log"
    # The naming server reads services.json from its working directory.
    (directory / "services.json").write_bytes(args.naming_config.read_bytes())

    broker = processes.enter_context(run_process([args.broker, BROKER], broker_log, directory))
    wait_for(
        "broker to listen",
        lambda: "Waiting for connection to accept!" in broker_log.read_text(),
        broker,
    )
    naming_server = processes.enter_context(run_process([args.naming], naming_log, directory))
    wait_for(
        "naming endpoint to register",
        lambda: re.search(
            rf"Registering .* as an endpoint for .*{NAMING_SERVICE_UUID}", broker_log.read_text()
        ),
        broker, naming_server,
    )

    services = [broker, naming_server]
    if "snmp" in plugin_names:
        services.append(start_snmp_agent(processes, directory, broker, naming_server))
    return broker_log, naming_log, services


def write_sensor_config(name, directory, sensor_uuid, **sensor_fields):
    sensor = {
        "uuid": sensor_uuid,
        "interval_ms": 250,
        "nameserver_parts": {"metric": name},
        **sensor_fields,
    }
    return write_config(directory / "plugin.yaml", {
        "name": name,
        "sensor_templates": {},
        "sensor_groups": {"plugin_test": {"sensors": [sensor]}},
    })


def collector_command(name, directory, install_prefix, plugin_yaml):
    plugin = next(install_prefix.glob(f"lib*/telemetry_collector/plugins/{name}.so"), None)
    if plugin is None:
        raise RuntimeError(f"{name}.so missing from {install_prefix}")

    config = write_config(directory / "telemetry_collector.yaml", {
        "seanerbus": {"url": ENDPOINT},
        "plugin_status_interval_ms": 100,
        "plugins": [{
            "path": str(plugin),
            "config": str(plugin_yaml),
            "uuid": PLUGIN_UUID,
            "name": name,
        }],
    })
    return [install_prefix / "bin/telemetry_collector", config]


def test_mock_loads_runs_and_unloads(fixture, directory):
    plugin_yaml = write_config(directory / "plugin.yaml", {"name": "mock"})
    command = collector_command("mock", directory, fixture.install_prefix, plugin_yaml)
    collector_log = directory / "collector.log"
    with run_process(command, collector_log, directory) as collector:
        wait_for(
            "mock runtime status",
            lambda: f"mock ({PLUGIN_UUID})" in collector_log.read_text(),
            collector, *fixture.services,
        )
        collector.terminate()
        exit_code = collector.wait(timeout=5)
        output = collector_log.read_text()
        if exit_code != 0 or "Stopping plugin:" not in output or "Unloading plugin:" not in output:
            raise RuntimeError("Mock did not run, stop, and unload cleanly")


def test_tester_publishes_five_counter_readings(fixture, directory):
    check_registered_readings(
        fixture, directory, "tester", "7bc3d210-a49f-4422-8fe8-26ca480d0e1a", 1, value_step=1
    )


def test_sysfs_reads_configured_path_and_publishes(fixture, directory):
    # A regular file gives the sysfs plugin a predictable value at its configured path.
    reading_file = directory / "reading"
    reading_file.write_text(f"{SYSFS_READING}\n")
    check_registered_readings(
        fixture, directory, "sysfs", "7bc3d210-a49f-4422-8fe8-26ca480d0e1b", SYSFS_READING,
        path=str(reading_file),
    )


def test_snmp_reads_configured_oid_and_publishes(fixture, directory):
    check_registered_readings(
        fixture, directory, "snmp", "7bc3d210-a49f-4422-8fe8-26ca480d0e1c", SNMP_READING,
        connection=SNMP_AGENT, version=2, community=SNMP_COMMUNITY, oid_suffix=SNMP_OID,
    )


def check_registered_readings(
    fixture, directory, name, sensor_uuid, first_value, *, value_step=0, **sensor_fields
):
    plugin_yaml = write_sensor_config(name, directory, sensor_uuid, **sensor_fields)
    command = collector_command(name, directory, fixture.install_prefix, plugin_yaml)
    subscriber_log = directory / "subscriber.log"
    subscriber_command = [
        fixture.subscriber, ENDPOINT, sensor_uuid, str(first_value), str(value_step)
    ]

    # Subscribe first so the subscriber can check the first five published values.
    with run_process(subscriber_command, subscriber_log, directory) as subscriber:
        wait_for(
            f"{name} subscription",
            lambda: re.search(rf"Subscribing .* to .*{sensor_uuid}", fixture.broker_log.read_text()),
            subscriber, *fixture.services,
        )
        with run_process(command, directory / "collector.log", directory) as collector:
            # The subscriber exits successfully only after validating all five readings.
            wait_for(f"{name} telemetry", lambda: subscriber.poll() is not None, collector, *fixture.services)
            if subscriber.returncode != 0:
                raise RuntimeError(f"{name} subscriber failed to validate five readings")

            wait_for(
                f"{name} registration",
                lambda: any(
                    "Registered " in line and sensor_uuid in line
                    for line in fixture.naming_log.read_text().splitlines()
                ),
                collector, *fixture.services,
            )
            print(f"{name}: {subscriber_log.read_text().strip()}", flush=True)


PLUGIN_TESTS = {
    "mock": test_mock_loads_runs_and_unloads,
    "tester": test_tester_publishes_five_counter_readings,
    "sysfs": test_sysfs_reads_configured_path_and_publishes,
    "snmp": test_snmp_reads_configured_oid_and_publishes,
}


def main():
    parser = argparse.ArgumentParser()
    for option in ("prefix", "broker", "naming", "naming-config", "subscriber", "logs"):
        parser.add_argument(f"--{option}", required=True, type=lambda path: Path(path).resolve())
    parser.add_argument("--plugins", default="mock;tester;sysfs;snmp")
    args = parser.parse_args()
    plugin_names = [name for name in args.plugins.split(";") if name in PLUGIN_TESTS]
    if not plugin_names:
        parser.error("select at least one of mock, tester, sysfs, or snmp")

    args.logs.mkdir(parents=True, exist_ok=True)
    failures = []
    with ExitStack() as processes:
        broker_log, naming_log, services = start_services(processes, args, plugin_names)
        fixture = PluginTestFixture(args.prefix, args.subscriber, broker_log, naming_log, services)
        for name in plugin_names:
            try:
                directory = args.logs / name
                directory.mkdir(exist_ok=True)
                PLUGIN_TESTS[name](fixture, directory)
                print(f"PASS {name}", flush=True)
            except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
                failures.append(name)
                print(f"FAIL {name}: {error}", flush=True)

    if failures:
        raise SystemExit(f"Failed plugin checks: {', '.join(failures)}")


if __name__ == "__main__":
    main()
