set(ctest_arguments --output-on-failure --output-log "${ARTIFACT_ROOT}/ctest.log" -L plugin_configuration)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
  list(APPEND ctest_arguments --output-junit "${ARTIFACT_ROOT}/junit.xml")
endif()

file(REMOVE_RECURSE "${ARTIFACT_ROOT}")
file(MAKE_DIRECTORY "${ARTIFACT_ROOT}")

execute_process(
  COMMAND "${CTEST_COMMAND}" ${ctest_arguments}
  WORKING_DIRECTORY "${BUILD_DIRECTORY}"
  RESULT_VARIABLE ctest_result)

set(REPORT_TITLE "Plugin configuration test report")
set(REPORT_CASES TRUE)
include("${CMAKE_CURRENT_LIST_DIR}/../reporting/write_suite_report.cmake")
get_filename_component(REPORT_ROOT "${ARTIFACT_ROOT}" DIRECTORY)
include("${CMAKE_CURRENT_LIST_DIR}/../reporting/write_report_index.cmake")

if(NOT "${ctest_result}" STREQUAL "0")
  message(FATAL_ERROR "CTest configuration tests failed with result: ${ctest_result}")
endif()
