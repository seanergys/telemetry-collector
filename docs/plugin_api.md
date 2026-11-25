<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: EUPL-1.2
-->

# Plugin API

This reference describes the plugin interface declared in
[`include/plugin.hpp`](../include/plugin.hpp).

## Exported symbol

A plugin is a shared library that implements the functions in
`plugin_interface_t`. It exports a table of those functions under the
unmangled symbol `plugin`. Use C linkage for the table so the manager can
find it with `dlsym`. Every function pointer in the table must be non-null.

```cpp
extern "C" {
plugin_interface_t plugin = {
    get_version,
    create,
    destroy,
    init_sensors,
    finit,
    run,
    configurator,
};
}
```

The functions shown are implemented by the plugin. The descriptor holds
their addresses, while the manager owns the runtime state.

## Plugin functions

| Member | Signature | When it runs and how the result is used |
| --- | --- | --- |
| `get_version` | `version_t* (void)` | Provides the plugin version. The loader checks that the function pointer is present. It does not call it or compare versions during loading. `version_t` exposes major, minor, and patch fields. |
| `configurator` | `void* (void*)` | Called during load, before `create`. The argument is a pointer to the parsed `YAML::Node`. The manager sets `seanerbus.url` on that node before calling the plugin. Return a non-null configuration pointer to accept it or `nullptr` to reject the load. |
| `create` | `void* (void)` | Called during initialization, before `init_sensors`. The manager discards the returned pointer and passes its own runtime record to subsequent plugin functions. |
| `init_sensors` | `bool (void*)` | Receives a `runtime_t*`. Set up the state needed by the worker. Returning `true` marks initialization successful. Returning `false` causes the manager to unload the plugin. |
| `run` | `void* (void*)` | Receives a `runtime_t*` and runs on a worker thread after successful initialization. The return value is discarded. The worker must return before the manager can finish stopping the plugin. |
| `finit` | `bool (void*)` | Receives a `runtime_t*` after the worker has stopped. Release state created during sensor initialization. The manager clears `runtime_t::config` before calling `finit` and discards its return value. |
| `destroy` | `bool (void*)` | Receives a `runtime_t*` immediately before the library is closed. It can be called for a loaded plugin that was never initialized or whose initialization failed. The manager discards its return value. |

See the [configuration reference](../config/README.md) for the YAML
parameters, or the [mock plugin](../plugins/mock/mock_plugin.cpp) for a
minimal exported descriptor.
