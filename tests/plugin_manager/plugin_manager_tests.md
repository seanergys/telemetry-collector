# PluginManager Test Summary

The table below follows the declaration order in
[`test_plugin_manager.cpp`](./test_plugin_manager.cpp). The summary IDs are
continuous and match the test IDs in that file.

## Plugin loading and interface validation

| Test ID | Test in `test_plugin_manager.cpp` | Scenario and asserted result |
|---------|-----------------------------------|-------------------------------|
| 001 | `001_LoadPlugin_MissingCreate_ShouldBeRejected` | Missing `create`: load returns false and the UUID is not loaded. |
| 002 | `002_LoadPlugin_MissingDestroy_ShouldBeRejected` | Missing `destroy`: load returns false and the UUID is not loaded. |
| 003 | `003_LoadPlugin_MissingInitFunctions_ShouldBeRejected` | Missing `init_sensors`: load returns false and the UUID is not loaded. |
| 004 | `004_LoadPlugin_MissingFinit_ShouldBeRejected` | Missing `finit`: load returns false and the UUID is not loaded. |
| 005 | `005_LoadPlugin_ValidPlugin_ShouldSucceed` | Valid plugin path and configuration: load succeeds and the UUID is loaded. |
| 006 | `006_LoadPlugin_MissingCreateAndDestroy_ShouldBeRejected` | Missing `create` and `destroy`: load returns false and the UUID is not loaded. |
| 007 | `007_LoadPlugin_MissingInitAndFinit_ShouldBeRejected` | Missing `init_sensors` and `finit`: load returns false and the UUID is not loaded. |
| 008 | `008_LoadPlugin_NonExistentFile_ShouldFail` | Nonexistent plugin-library path: load returns false and the UUID is not loaded. |
| 009 | `009_LoadPlugin_InvalidConfig_ShouldFail` | Invalid YAML configuration: load returns false and the UUID is not loaded. |
| 010 | `010_LoadPlugin_MissingConfigFile_ShouldFail` | Nonexistent configuration-file path: load returns false and the UUID is not loaded. |
| 011 | `011_LoadPlugin_SameLibraryDifferentUUIDs_ShouldSucceed` | Load one library under two UUIDs: both loads succeed and both UUIDs are loaded. |
| 012 | `012_LoadPlugin_SameUUID_ShouldFail` | Load one UUID twice: first load succeeds, second fails, and the UUID remains loaded. |

## Initialization and finalization

| Test ID | Test in `test_plugin_manager.cpp` | Scenario and asserted result |
|---------|-----------------------------------|-------------------------------|
| 013 | `013_InitPlugin_WithSensors_ShouldSucceed` | Initialize a loaded plugin with sensors: init and finit succeed; it remains loaded until manager cleanup. |
| 014 | `014_InitPlugin_NonExistentPlugin_ShouldFail` | Initialize a nonexistent UUID: returns false. |
| 015 | `015_InitPlugin_NotLoaded_ShouldFail` | Initialize a UUID that was not loaded: returns false. |
| 016 | `016_InitPlugin_AlreadyInitialized_ShouldFail` | Initialize twice: first call succeeds and second fails; finalization and unload succeed. |
| 017 | `017_InitPlugin_RunningPlugin_ShouldFail` | Re-initialize while running: returns false; plugin remains loaded and can then be stopped and finalized. |
| 018 | `018_FinitPlugin_ValidPlugin_ShouldSucceed` | Finalize an initialized plugin: succeeds, then unload succeeds. |
| 019 | `019_FinitPlugin_AlreadyFinit_ShouldFail` | Finalize twice: first succeeds and second fails. |
| 020 | `020_FinitPlugin_NonExistentPlugin_ShouldFail` | Finalize a nonexistent UUID: returns false. |

## Start and stop

| Test ID | Test in `test_plugin_manager.cpp` | Scenario and asserted result |
|---------|-----------------------------------|-------------------------------|
| 021 | `021_StartPlugin_ValidPlugin_ShouldSucceed` | Start an initialized plugin: start, stop, finit, and unload all succeed. |
| 022 | `022_StartPlugin_NonExistentPlugin_ShouldFail` | Start a nonexistent UUID: returns false. |
| 023 | `023_StartPlugin_NotInitialized_ShouldFail` | Start a loaded but uninitialized plugin: returns false and the plugin remains loaded. |
| 024 | `024_StartPlugin_AlreadyStarted_ShouldFail` | Start twice: first succeeds and second fails; cleanup succeeds. |
| 025 | `025_StartPlugin_UnloadedPlugin_ShouldFail` | Unload a plugin, then start it: start and second unload both return false; it remains unloaded. |
| 026 | `026_StopPlugin_RunningPlugin_ShouldSucceed` | Stop a running plugin: stop, finit, and unload succeed. |
| 027 | `027_StopPlugin_NonExistentPlugin_ShouldFail` | Stop a nonexistent UUID: returns false. |
| 028 | `028_StopPlugin_NotRunning_ShouldFail` | Stop an initialized but non-running plugin: returns false. |
| 029 | `029_StopPlugin_AlreadyStopped_ShouldFail` | Stop twice: first succeeds and second fails; finalization and unload succeed. |

## Unloading

| Test ID | Test in `test_plugin_manager.cpp` | Scenario and asserted result |
|---------|-----------------------------------|-------------------------------|
| 030 | `030_UnloadPlugin_ValidPlugin_ShouldSucceed` | Unload a loaded plugin: succeeds and the UUID is no longer loaded. |
| 031 | `031_UnloadPlugin_NonExistentPlugin_ShouldFail` | Unload a nonexistent UUID: returns false. |
| 032 | `032_UnloadPlugin_AlreadyUnloaded_ShouldFail` | Unload twice after a complete lifecycle: first succeeds and second fails. |
| 033 | `033_UnloadPlugin_RunningPlugin_ShouldFail` | Unload a running plugin: returns false and it remains loaded; subsequent stop and finit are attempted. |
| 034 | `034_UnloadPlugin_InitializedPlugin_ShouldFail` | Unload an initialized plugin: returns false and it remains loaded; subsequent finit is attempted. |

## Complete lifecycle and multiple-plugin scenarios

| Test ID | Test in `test_plugin_manager.cpp` | Scenario and asserted result |
|---------|-----------------------------------|-------------------------------|
| 035 | `035_CompleteLifecycle_ShouldSucceed` | Load, init, start, stop, finit, and unload: each operation succeeds; UUID is no longer loaded. |
| 036 | `036_MultiplePlugins_MixedOperations_ShouldSucceed` | Five plugins with mixed lifecycle operations: checks valid transitions, failure to finit a running plugin, and final loaded states. |
| 037 | `037_MultiplePlugins_StartAllStopReverse_ShouldSucceed` | Load, initialize, and start three plugins; stop in reverse order, then finit and unload all successfully. |
| 038 | `038_MultiplePlugins_RandomUnloadThenOperate_ShouldSucceed` | Load four plugins; unload selected plugins at different lifecycle stages; remaining plugins complete their lifecycles. |
| 039 | `039_MultiplePlugins_StartMiddleStopEnds_ShouldSucceed` | Start the middle three of five; stopping the non-running ends returns false; stop, finit, and unload then succeed for all. |
| 040 | `040_MultiplePlugins_StartWithoutInit_ShouldHandleGracefully` | Attempt to start a loaded, uninitialized plugin; the result is stored but not asserted. After init, start and cleanup are asserted successful. |
| 041 | `041_MultiplePlugins_UnloadMiddleVerifyRemaining_ShouldSucceed` | Finit and unload the middle two of four; verify the other two remain loaded and complete their lifecycles. |
| 042 | `042_MultiplePlugins_AlternatingOperations_ShouldSucceed` | Interleave operations across five plugins; check expected failures for invalid transitions and successful cleanup. Some early operation results are not asserted. |
| 043 | `043_MultiplePlugins_UnloadWhileOthersRunning_ShouldSucceed` | Unload of a running plugin fails; after stopping and finalizing it, unload succeeds while the other plugins remain loaded. |
| 044 | `044_MultiplePlugins_InitReverseStartOriginal_ShouldSucceed` | Initialize four plugins in reverse order, start in original order, then stop, finit, and unload all. |

## Repeated operations and invalid-state handling

| Test ID | Test in `test_plugin_manager.cpp` | Scenario and asserted result |
|---------|-----------------------------------|-------------------------------|
| 045 | `045_RedundantLoad_SameUUID_ShouldFail` | Load the same UUID twice: first succeeds and second fails. |
| 046 | `046_RedundantInit_ShouldFail` | Initialize twice: first succeeds and second fails; finalization and unload succeed. |
| 047 | `047_RedundantStart_ShouldFail` | Start twice: first succeeds and second fails; stop, finit, and unload succeed. |
| 048 | `048_RedundantStop_ShouldFail` | Stop twice: first succeeds and second fails; finit and unload succeed. |
| 049 | `049_RedundantFinit_ShouldFail` | Finit twice: first succeeds and second fails; unload succeeds. |
| 050 | `050_RedundantUnload_ShouldFailAfterFirst` | Unload twice: first succeeds and second fails; UUID is no longer loaded. |
| 051 | `051_RedundantLifecycle_ShouldSucceed` | Complete the same load-to-unload lifecycle twice with the same UUID: both rounds succeed. |
| 052 | `052_RedundantOperations_NonExistentPlugin_ShouldFailGracefully` | Init, start, stop, finit, and unload a nonexistent UUID: all return false. |

## Metadata queries and plugin reporting

| Test ID | Test in `test_plugin_manager.cpp` | Scenario and asserted result |
|---------|-----------------------------------|-------------------------------|
| 053 | `053_RedundantGetCapabilities_ShouldBeConsistent` | Query capabilities three times: result sizes match; unload succeeds. |
| 054 | `054_RedundantGetLoadedPlugins_ShouldBeConsistent` | Query loaded UUIDs three times: result sizes match; unload succeeds. |
| 055 | `055_RedundantPrintLoadedPlugins_ShouldNotCrash` | Print loaded-plugin information three times: no crash is asserted; unload is called. |
| 056 | `056_GetPluginCapabilities_ValidPlugin_ShouldReturnCapabilities` | Query a loaded plugin: capabilities are non-empty and unload succeeds. |
| 057 | `057_GetPluginCapabilities_NonExistentPlugin_ShouldReturnEmpty` | Query a nonexistent UUID: capabilities are empty. |
| 058 | `058_GetPluginCapabilities_UnloadedPlugin_ShouldReturnEmpty` | Query after unload: capabilities are empty. |
| 059 | `059_GetPluginCapabilities_Consistency_ShouldMatch` | Query twice: capability list sizes and each corresponding value match. |
| 060 | `060_GetLoadedPlugins_ValidPlugins_ShouldReturnList` | Load two plugins: returned list contains both UUIDs. |
| 061 | `061_GetLoadedPlugins_NoPlugins_ShouldReturnEmpty` | With no plugins loaded: returned list is empty. |
| 062 | `062_GetLoadedPlugins_AfterUnload_ShouldReflectChanges` | Query before and after unloading one plugin: list size changes from one to zero. |
| 063 | `063_GetLoadedPlugins_Uniqueness_ShouldNotDuplicate` | Attempt to load a UUID twice: returned list contains no duplicate UUIDs. |
| 064 | `064_PrintLoadedPlugins_NoPlugins_ShouldNotCrash` | Print with no plugins loaded: no crash is asserted. |
| 065 | `065_PrintLoadedPlugins_WithPlugins_ShouldNotCrash` | Print with a plugin loaded: no crash is asserted; unload is called. |
| 066 | `066_PrintLoadedPlugins_LifecycleChanges_ShouldReflect` | Print with no plugin, through each lifecycle stage, and after unload: no crash is asserted. |

## `run` callback requirements and lifecycle

| Test ID | Test in `test_plugin_manager.cpp` | Scenario and asserted result |
|---------|-----------------------------------|-------------------------------|
| 067 | `067_LoadPlugin_MissingRun_ShouldBeRejected` | Plugin missing `run`: load returns false and the UUID is not loaded. |
| 068 | `068_LoadPlugin_WithRun_ShouldSucceed` | Plugin with `run`: load succeeds and the UUID is loaded. |
| 069 | `069_InitPlugin_WithRun_ShouldSucceed` | Plugin with `run`: load, init, and finit succeed; it remains loaded after finit. |
| 070 | `070_StartStopPlugin_WithRun_ShouldSucceed` | Plugin with `run`: start, stop, finit, and unload succeed. |
| 071 | `071_GetCapabilities_WithRun_ShouldIncludeRun` | Plugin with `run`: capabilities include `run`; unload succeeds. |
| 072 | `072_GetCapabilities_NoRun_ShouldNotIncludeRun` | Plugin without `run`: capabilities do not include `run`; `load_plugin` result is not asserted and unload is asserted false. |
| 073 | `073_CompleteLifecycle_WithRun_ShouldSucceed` | Plugin with `run`: load, init, start, stop, finit, and unload all succeed. |
| 074 | `074_PeriodicStatusReporting_ReflectsPluginLoadAndUnload` | Periodic reports reflect zero, one, and two loaded plugins, then update after each unload. |

## Per-test artifacts

`gtest_discover_tests` registers each case with CTest, so CTest and GitLab report
the cases individually. Configure only this group with:

```sh
cmake -S . -B build-manager \
  -DTC_BUILD_PLUGIN_MANAGER_TESTS=ON \
  -DTC_BUILD_PLUGIN_CONFIGURATION_TESTS=OFF
```

Then run:

```sh
cmake --build build-manager --target run_plugin_manager_tests --parallel 4
```

It writes `build-manager/test-artifacts/plugin-manager/junit.xml`. Config
snapshots are saved under
`build-manager/test-artifacts/plugin-manager/PluginManagerTest.<test-name>/configs/`.
CTest records each case separately. Temporary YAML files in the test working
directory are removed after the run.

Open `test-artifacts/plugin-manager/report.md` in the build directory for the suite log, JUnit results, and available per-case logs and config snapshots.
