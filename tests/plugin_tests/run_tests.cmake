# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: EUPL-1.2

set(ctest_arguments --output-on-failure --output-log "${ARTIFACT_ROOT}/ctest.log" -L "^plugin_tests$")
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
  list(APPEND ctest_arguments --output-junit "${ARTIFACT_ROOT}/junit.xml")
endif()

file(MAKE_DIRECTORY "${ARTIFACT_ROOT}")
execute_process(
  COMMAND "${CTEST_COMMAND}" ${ctest_arguments}
  WORKING_DIRECTORY "${BUILD_DIRECTORY}"
  RESULT_VARIABLE ctest_result)

function(log_link output label relative_path)
  if(EXISTS "${ARTIFACT_ROOT}/${relative_path}")
    set("${output}" "[${label}](${relative_path})" PARENT_SCOPE)
  else()
    set("${output}" "—" PARENT_SCOPE)
  endif()
endfunction()

set(REPORT_TITLE "Plugin test report")
include("${CMAKE_CURRENT_LIST_DIR}/../reporting/write_suite_report.cmake")

file(APPEND "${report}"
  "| Plugin | Telemetry collector | Naming server | SeanerBUS broker | SNMP agent | Subscriber | Collector config | Plugin config |\n"
  "| --- | --- | --- | --- | --- | --- | --- | --- |\n")
string(REPLACE "," ";" plugins "${PLUGIN_TEST_NAMES}")
foreach(plugin IN LISTS plugins)
  log_link(collector "Collector" "${plugin}/${plugin}/collector.log")
  log_link(naming "Naming" "${plugin}/naming.log")
  log_link(broker "Broker" "${plugin}/broker.log")
  log_link(snmp "SNMP agent" "${plugin}/snmpd.log")
  log_link(subscriber "Subscriber" "${plugin}/${plugin}/subscriber.log")
  log_link(collector_config "YAML" "${plugin}/${plugin}/telemetry_collector.yaml")
  log_link(plugin_config "YAML" "${plugin}/${plugin}/plugin.yaml")
  file(APPEND "${report}"
    "| ${plugin} | ${collector} | ${naming} | ${broker} | ${snmp} | ${subscriber} | ${collector_config} | ${plugin_config} |\n")
endforeach()
message(STATUS "Plugin test report: ${report}")
get_filename_component(REPORT_ROOT "${ARTIFACT_ROOT}" DIRECTORY)
include("${CMAKE_CURRENT_LIST_DIR}/../reporting/write_report_index.cmake")

if(NOT "${ctest_result}" STREQUAL "0")
  message(FATAL_ERROR "CTest plugin tests failed with result: ${ctest_result}")
endif()
