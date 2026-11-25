# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: EUPL-1.2

find_package(Python3 3.6 REQUIRED COMPONENTS Interpreter)

set(TC_PLUGIN_TEST_BROKER "" CACHE FILEPATH "Path to the SeanerBUS broker executable")
set(TC_PLUGIN_TEST_NAMING "" CACHE FILEPATH "Path to the naming server executable")
set(TC_PLUGIN_TEST_NAMING_CONFIG "" CACHE FILEPATH "Path to the naming server services.json")
set(TC_PLUGIN_TEST_INSTALL_PREFIX "" CACHE PATH
    "Override the CMAKE_INSTALL_PREFIX used by the plugin tests")
set(plugin_test_install_prefix "${TC_PLUGIN_TEST_INSTALL_PREFIX}")
if(NOT plugin_test_install_prefix)
  set(plugin_test_install_prefix "${CMAKE_INSTALL_PREFIX}")
endif()

foreach(variable IN ITEMS TC_PLUGIN_TEST_BROKER TC_PLUGIN_TEST_NAMING
                          TC_PLUGIN_TEST_NAMING_CONFIG)
  if(NOT EXISTS "${${variable}}" OR IS_DIRECTORY "${${variable}}")
    message(FATAL_ERROR "Set ${variable} to an existing file for the plugin tests")
  endif()
endforeach()

add_executable(test_plugin_subscriber subscribe.cpp)
target_compile_features(test_plugin_subscriber PRIVATE cxx_std_20)
target_link_libraries(test_plugin_subscriber PRIVATE PkgConfig::SEANERBUS)

set(artifact_root "${CMAKE_BINARY_DIR}/test-artifacts/plugin-tests")
set(plugin_test_cases)
function(add_plugin_test plugin test_case)
  if(NOT TARGET "${plugin}")
    return()
  endif()

  set(plugin_test_cases ${plugin_test_cases} "${plugin}" PARENT_SCOPE)
  set(test_name "PluginTest.${test_case}")
  add_test(
    NAME "${test_name}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run.py"
      --prefix "${plugin_test_install_prefix}"
      --broker "${TC_PLUGIN_TEST_BROKER}"
      --naming "${TC_PLUGIN_TEST_NAMING}"
      --naming-config "${TC_PLUGIN_TEST_NAMING_CONFIG}"
      --subscriber "$<TARGET_FILE:test_plugin_subscriber>"
      --logs "${artifact_root}/${plugin}"
      --plugins "${plugin}")
  set_tests_properties("${test_name}" PROPERTIES
    LABELS plugin_tests
    RESOURCE_LOCK seanerbus_plugin_test_ports
    ENVIRONMENT "RUST_LOG=debug"
    TIMEOUT 150)
endfunction()

add_plugin_test(mock MockLoadsRunsAndUnloads)
add_plugin_test(tester TesterPublishesFiveCounterReadings)
add_plugin_test(sysfs SysfsReadsConfiguredPathAndPublishes)
add_plugin_test(snmp SnmpReadsConfiguredOidAndPublishes)

if(NOT plugin_test_cases)
  message(FATAL_ERROR "Plugin tests require at least one of mock, tester, sysfs, or snmp")
endif()

string(JOIN "," plugin_test_names ${plugin_test_cases})
add_custom_target(
  run_plugin_tests
  COMMAND "${CMAKE_COMMAND}"
    "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
    "-DARTIFACT_ROOT=${artifact_root}"
    "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
    "-DPLUGIN_TEST_NAMES=${plugin_test_names}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_tests.cmake"
  DEPENDS test_plugin_subscriber
  USES_TERMINAL
  VERBATIM)
