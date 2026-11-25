<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: EUPL-1.2
-->

# Telemetry Collector configuration

The collector reads a main YAML file that selects the broker and plugin
instances. Each plugin instance points to a second YAML file with its own
settings. Installed examples are under `etc/telemetry_collector/`. See
[RUN.md](../RUN.md) for launching the collector and services.

## Contents

- [Global configuration — `config.yaml`](#global-configuration--configyaml)
- [Common plugin parameters](#common-plugin-parameters)
- [Sysfs — `sysfs.yaml`](#sysfs--sysfsyaml)
- [SNMP — `snmp.yaml`](#snmp--snmpyaml)
- [AD — `ad.yaml`](#ad--adyaml)
- [Tester — `tester.yaml`](#tester--testeryaml)
- [Mock — `mock.yaml`](#mock--mockyaml)

## Global configuration — `config.yaml`

CMake generates the main configuration from [`config.yaml.in`](config.yaml.in).
It fills in the installed library and configuration paths. Copy the generated
file to a writable location and select the entries for the plugins you built.

| Parameter | Type or default | Behavior |
| --- | --- | --- |
| `seanerbus.url` | Required non-empty string | Broker address shared by the configured plugins. Use `seanerbroker://host:port` or `host:port`. The collector prepends the scheme if it is absent. The manager supplies this URL to each plugin and replaces any `seanerbus.url` in its YAML. |
| `plugin_status_interval_ms` | Required non-negative integer in milliseconds. The shipped file uses `1000`. | How often the collector logs which plugins are loaded at info level. Use `0` to disable these reports. |
| `plugins` | Sequence of mappings | One entry per plugin instance to load at startup. Use `[]` for an empty list. |
| `plugins[].path` | Required string | Shared library passed to the dynamic loader, such as the installed `sysfs.so`. This field selects the implementation. |
| `plugins[].config` | Required string | Path to the plugin YAML file. Relative file paths are resolved from the collector process's working directory. |
| `plugins[].uuid` | Required string | Unique key for the plugin instance in the manager. This is independent of the sensor UUIDs used to publish data. Loading a second instance with the same key is rejected. |
| `plugins[].name` | Required string | Human-readable plugin name used in startup and status logs. |

See the [collector parser](../src/config.cpp) and
[plugin loader](../src/pluginManager.cpp).

## Common plugin parameters

Sysfs, SNMP, and Tester use the shared parser in
[`plugin_common.hpp`](../plugins/plugin_common.hpp). AD reuses its field
handlers after discovering sensors. For each configured Sysfs, SNMP, or
Tester sensor, the parser applies fields in this order:

1. The group’s template.
2. Fields directly on the group.
3. The sensor’s template.
4. Fields directly on the sensor.

Later values override earlier ones. `nameserver_parts` entries merge by key.
If a template reference is missing or unknown, the parser continues applying
the other layers. When a templates map exists but lacks the named entry, it
logs a warning. For these three plugins, supply a valid `log_level` and keep
`sensor_templates: {}` if you have no templates. The shared configurator can
log diagnostics before the plugin initializes its logger.

| Parameter | Scope and default | Behavior |
| --- | --- | --- |
| `log_level` | Plugin file root. The logger starts at `trace` when no level is set. | Accepts `trace`, `debug`, `info`, `warning` (or `warn`), `error` (or `err`), `critical`, and `off`, case-insensitively. An unknown level leaves the logger level unchanged when the logger is initialized. |
| `sensor_templates` | Plugin file root, optional mapping | Named reusable sets of sensor fields. |
| `sensor_groups` | Plugin file root, mapping | Groups sensors under names of your choice. Names are used in configuration diagnostics. A missing map produces no sensor configurations. |
| `template` | Group or sensor, optional string | Name of a template in `sensor_templates`. |
| `sensors` | Group, sequence | Defines the sensors in the group. An empty or missing list yields no sensors for that group. |
| `uuid` | Template, group, or sensor in Sysfs, SNMP, and Tester | Identifies the SeanerBUS publication destination. The common parser generates a UUID if none remains after all layers. SNMP and Tester replace malformed values with a generated UUID. See the Sysfs and AD sections for UUIDs obtained from the naming service. |
| `interval_ms` | Template, group, or sensor. Runtime default `0`. | Time in milliseconds between scheduled readings. The first reading is scheduled immediately. Set a positive interval for periodic readings. Zero leaves the next reading due immediately. |
| `nameserver_parts` | Template, group, or sensor, optional mapping in Sysfs, SNMP, and Tester | Name terms such as `metric`, `rack`, or `hostname` used for registration. The plugin fills in the local hostname if the configured hostname is absent or empty. Sysfs also looks up these terms before registration. AD has its own group-level discovery rules. |
| `description` | Template, group, or sensor, optional string | Stores a human-readable description. The description is separate from the published `TimeValueV1` timestamp and number. |
| `unit` | Template, group, or sensor, optional string | Stores a unit label. A unit label leaves the numeric value unchanged. |
| `topic` | Optional string in Sysfs, SNMP, and AD | Stores a label. The sensor UUID selects the publication destination. |
| `filter` | Optional string in SNMP and AD | Stored as configuration metadata. These plugins publish the numeric response directly. Sysfs applies its own filter behavior. |
| `factor` | Number in SNMP and AD, default `1` | Stored as configuration metadata. These plugins publish the unscaled response. Sysfs applies its own multiplier. |
| `offset` | Number in SNMP and AD, default `0` | Stored as configuration metadata. These plugins publish the response without an additive adjustment. Sysfs applies its own offset. |
| Root-level `name` | Optional example field | Set the plugin instance name in the main configuration. The plugin files do not use this field for identity. |
| Root-level `uuid` | Optional example field | Set the plugin instance UUID in the main configuration. A sensor UUID goes in a sensor field or an inherited layer. |

The shared parser applies a group-level UUID as a default for **each** sensor.
It does not enable batching. Give each sensor its own UUID or omit the UUID
where you want the parser to generate one. Duplicate UUIDs use the same map
entry, with the later configuration replacing the earlier one.

For AD, shared numeric and text fields such as `interval_ms`, `description`,
and `unit` can come from a group template or directly from the group. AD
applies them after discovery, and a group value takes precedence over the
template. The plugin sections below list settings whose behavior differs.

## Sysfs — `sysfs.yaml`

[Example YAML](sysfs.yaml) · [Implementation](../plugins/sysfs/sysfs_plugin.cpp)

Reads one line from a local file, such as a Linux attribute in `/sys` or
`/proc/loadavg`, and publishes a numeric value.

Use the [common plugin parameters](#common-plugin-parameters) for
inheritance and scheduling. The following settings have Sysfs-specific behavior.

### Sysfs-specific behavior

| Parameter | Type or default | Behavior |
| --- | --- | --- |
| `path` | Required string after inheritance | File to open for each reading. A sensor with a missing, empty, or nonexistent path is skipped during configuration. The file must also be readable when the plugin runs. |
| `filter` | Template, group, or sensor, optional string | When set, the plugin searches the first line and passes the first capture group to numeric conversion. Include a capture group that yields a number. Without a filter, the line is passed directly to numeric conversion. A missing capture or a non-numeric capture can make the conversion fail. |
| `factor` | Template, group, or sensor, number, default `1` | Multiplies the parsed numeric value. For example, `0.001` converts a millidegree reading to degrees. |
| `offset` | Template, group, or sensor, number, default `0` | Adds a constant after scaling. The published value is `parsed × factor + offset`. |
| `uuid` | Template, group, or sensor | A valid explicit UUID is kept. Sysfs ignores malformed UUID overrides. It adopts a resolved UUID when none was explicit, and a conflict with an explicit UUID fails initialization. If no name resolves, Sysfs registers the configured or generated UUID. |

For example, the shipped `loadavg` group overrides its template’s path with
`/proc/loadavg`. Each sensor’s filter selects a different number from that
same line.

### Example — one load-average sensor

The following two files configure a single Sysfs sensor. Adjust the library
path to match your installation. For example, some installations use `lib64`
in place of `lib`. Run the collector from the directory containing these two
files so the relative paths resolve.

`config.yaml`:

```yaml
seanerbus:
  url: seanerbroker://127.0.0.1:5398
plugin_status_interval_ms: 1000
plugins:
  - path: ./install/lib/telemetry_collector/plugins/sysfs.so
    config: ./sysfs.yaml
    uuid: e94e6e91-a83d-4e04-a147-aab3860d0cd7
    name: sysfs
```

`sysfs.yaml`:

```yaml
log_level: info
sensor_templates:
  loadavg:
    interval_ms: 2000
    path: /proc/loadavg
sensor_groups:
  load:
    template: loadavg
    sensors:
      - uuid: 4bc3d210-a49f-4422-8fe8-26ca480d0e1f
        nameserver_parts:
          metric: load_1min
        filter: '^(\d+\.\d+)'
```

The sensor inherits the path and interval from the template. The regular
expression captures the first number on the line, and the plugin publishes
it every two seconds using the sensor UUID. The collector and sensor UUIDs
serve different purposes. The first identifies the loaded plugin instance,
while the second identifies the publication destination.

## SNMP — `snmp.yaml`

[Example YAML](snmp.yaml) · [Implementation](../plugins/snmp/snmp_plugin.cpp)

Polls an agent with an SNMP GET. The reading path handles integer, counter,
gauge, time ticks, and unsigned integer response types.

Use the [common plugin parameters](#common-plugin-parameters) for
templates, sensor identity, and publication intervals.

### SNMP-specific settings

| Parameter | Type or default | Behavior |
| --- | --- | --- |
| `connection` | Required non-empty string after inheritance | Agent address passed to Net-SNMP, for example `127.0.0.1:1161`. A sensor without an address is skipped. |
| `oid_prefix` | String, optional if the suffix contains the full OID | Common leading portion of the OID. Useful in a template shared by several sensors. |
| `oid_suffix` | String, optional if the prefix contains the full OID | Trailing portion of the OID. The plugin concatenates prefix and suffix verbatim, so include a dot where the two portions meet. Both empty causes the sensor to be skipped during configuration. An invalid combined OID causes it to be skipped during initialization. |
| `version` | Use `1`, `2` or `2c`, or `3` | Selects SNMPv1, SNMPv2c, or SNMPv3. Set it explicitly. The parser reads the leading number, which is why `2c` selects v2c. |
| `community` | String for SNMPv1/v2c | Community string sent with the request. Set the value expected by the agent. |
| `type` | Optional string, such as `Agent` | Stored in the sensor configuration. It does not change the request type. |
| `oidPrefix` | Optional legacy spelling | Stored separately. Use `oid_prefix` and `oid_suffix` to control the actual GET OID. |

### SNMPv3 fields

The parser handles these fields, but authenticated modes have a configuration
limitation. The implementation casts numeric `authProto` and `privProto`
scalars to pointers rather than parsing protocol OIDs. Ordinary YAML
therefore cannot safely supply protocol OIDs for those modes.

| Parameter | Behavior |
| --- | --- |
| `username` | Security name used for the SNMPv3 session. |
| `securityLevel` | Integer passed to Net-SNMP. The implementation branches on no authentication, authentication without privacy, or authentication with privacy. |
| `authKey` | Passphrase used to derive the authentication key. |
| `privKey` | Passphrase used to derive the privacy key when privacy is enabled. |
| `authProto` | Input used as the authentication protocol pointer. Subject to the YAML limitation above. |
| `authProtoLen` | Length passed to Net-SNMP with the authentication protocol pointer. |
| `privProto` | Input used as the privacy protocol pointer. Subject to the YAML limitation above. |
| `privProtoLen` | Length passed to Net-SNMP with the privacy protocol pointer. |

## AD — `ad.yaml`

[Example YAML](ad.yaml) · [Implementation](../plugins/ad/ad_plugin.cpp)

Discovers remote sensor entities through the SeanerBUS naming service.
It creates a sensor for each matched entity, requests a reading via
SeanerBUS, and republishes the returned value and timestamp. The naming
service and the remote request responders must be available.

See the [common plugin parameters](#common-plugin-parameters) for the
fields AD shares with other plugins. The following settings control discovery
or differ from the shared parser.

### AD discovery settings

| Parameter | Type or default | Behavior |
| --- | --- | --- |
| `seanerbus.url` | Required at runtime, supplied by the collector | Broker used for name lookup and requests. The main configuration supplies it before the configurator runs. |
| `sensor_groups` | Required mapping | Each group describes a discovery query. If all queries yield no sensors, the configurator rejects the plugin configuration. |
| `sensor_groups.<group>.nameserver_parts` | Required mapping on the group itself | Terms sent in the lookup, such as `hostname`. Put lookup terms directly on the group because discovery runs before template fields are applied. A group without this mapping is skipped. |
| `sensors` | Unused by AD | The naming service returns the sensors for each group. |
| `connection_uuid` | Unused by AD | The commented example field has no effect on the connection or request target. |
| `template` | Group, optional string | Selects defaults applied after discovery, before the fields directly on the group. |
| `uuid` | Normally omit from group and template | AD initializes each sensor’s UUID from discovery. An explicit UUID in the applied fields overrides the UUID used for requests and publication. A malformed value generates a UUID. Leave it unset to retain each discovered entity’s UUID. |

## Tester — `tester.yaml`

[Example YAML](tester.yaml) · [Implementation](../plugins/tester/tester_plugin.cpp)

Publishes a separate counter for each sensor. The counter starts at zero and
is incremented before each publication, so a newly initialized sensor
publishes `1` first.

Use the [common plugin parameters](#common-plugin-parameters) to configure
the counter sensors. Each counter increments once per `interval_ms`. The
shipped temperature sensor sets its interval to `1000` ms.

### Settings disabled in Tester

| Parameter | Behavior |
| --- | --- |
| `topic` | Disabled in Tester’s shared field parser. |
| `filter` | Disabled in Tester’s shared field parser. |
| `factor` | Disabled in Tester’s shared field parser. The example’s template multiplier has no effect. |
| `offset` | Disabled in Tester’s shared field parser. |

## Mock — `mock.yaml`

[Example YAML](mock.yaml) · [Implementation](../plugins/mock/mock_plugin.cpp)

Mock exercises the plugin lifecycle. Its configurator returns the YAML
unchanged, its initialization creates no sensors, and it publishes no
readings.

| Parameter | Behavior |
| --- | --- |
| `name` | Unused example field. Set the instance name in the main configuration. |
| `uuid` | Unused example field. Set the instance UUID in the main configuration. |
| `sensors` | Unused example field. Mock creates no sensors. |


