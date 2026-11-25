set(PLUGIN_MANAGER_FIXTURE_DIR "${CMAKE_BINARY_DIR}/plugins")
set(PLUGIN_MANAGER_ARTIFACT_ROOT
    "${CMAKE_BINARY_DIR}/test-artifacts/plugin-manager")

function(add_plugin_manager_fixture target output_name)
  add_library(
    "${target}" SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/plugin_manager_fixture.cpp")
  set_target_properties(
    "${target}"
    PROPERTIES OUTPUT_NAME "${output_name}"
               PREFIX ""
               LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_MANAGER_FIXTURE_DIR}")
  target_compile_features("${target}" PRIVATE cxx_std_20)
  target_include_directories(
    "${target}" PRIVATE "${PROJECT_SOURCE_DIR}/include")
  if(ARGN)
    target_compile_definitions("${target}" PRIVATE ${ARGN})
  endif()
endfunction()

add_plugin_manager_fixture(plugin_manager_fixture_test test)
add_plugin_manager_fixture(plugin_manager_fixture_with_run test_plugin_with_run)
add_plugin_manager_fixture(
  plugin_manager_fixture_no_create
  test_plugin_no_create
  TEST_PLUGIN_MISSING_CREATE)
add_plugin_manager_fixture(
  plugin_manager_fixture_no_destroy
  test_plugin_no_destroy
  TEST_PLUGIN_MISSING_DESTROY)
add_plugin_manager_fixture(
  plugin_manager_fixture_no_init
  test_plugin_no_init
  TEST_PLUGIN_MISSING_INIT_SENSORS)
add_plugin_manager_fixture(
  plugin_manager_fixture_no_finit
  test_plugin_no_finit
  TEST_PLUGIN_MISSING_FINIT)
add_plugin_manager_fixture(
  plugin_manager_fixture_no_create_destroy
  test_plugin_no_create_destroy
  TEST_PLUGIN_MISSING_CREATE
  TEST_PLUGIN_MISSING_DESTROY)
add_plugin_manager_fixture(
  plugin_manager_fixture_no_init_finit
  test_plugin_no_init_finit
  TEST_PLUGIN_MISSING_INIT_SENSORS
  TEST_PLUGIN_MISSING_FINIT)
add_plugin_manager_fixture(
  plugin_manager_fixture_no_run
  test_plugin_no_run
  TEST_PLUGIN_MISSING_RUN)

add_executable(
  test_plugin_manager
  test_plugin_manager.cpp
  "${PROJECT_SOURCE_DIR}/src/config.cpp"
  "${PROJECT_SOURCE_DIR}/src/pluginManager.cpp"
  "${PROJECT_SOURCE_DIR}/src/logger.cpp")
target_compile_features(test_plugin_manager PRIVATE cxx_std_20)
target_include_directories(
  test_plugin_manager PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(test_plugin_manager PRIVATE SPDLOG_FMT_EXTERNAL)
target_link_libraries(
  test_plugin_manager
  PRIVATE ${TELEMETRY_COLLECTOR_GTEST_TARGET}
          PkgConfig::SEANERBUS
          PkgConfig::UUID
          yaml-cpp::yaml-cpp
          spdlog::spdlog
          ${CMAKE_DL_LIBS})
add_dependencies(
  test_plugin_manager
  plugin_manager_fixture_test
  plugin_manager_fixture_with_run
  plugin_manager_fixture_no_create
  plugin_manager_fixture_no_destroy
  plugin_manager_fixture_no_init
  plugin_manager_fixture_no_finit
  plugin_manager_fixture_no_create_destroy
  plugin_manager_fixture_no_init_finit
  plugin_manager_fixture_no_run)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests" "${PLUGIN_MANAGER_ARTIFACT_ROOT}")
gtest_discover_tests(
  test_plugin_manager
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
  PROPERTIES
    LABELS plugin_manager
    ENVIRONMENT "PLUGIN_TEST_ARTIFACT_ROOT=${PLUGIN_MANAGER_ARTIFACT_ROOT}")

add_custom_target(
  run_plugin_manager_tests
  COMMAND
    "${CMAKE_COMMAND}"
    "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
    "-DARTIFACT_ROOT=${PLUGIN_MANAGER_ARTIFACT_ROOT}"
    "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
    "-DREPORT_PYTHON=${Python3_EXECUTABLE}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_tests.cmake"
  DEPENDS test_plugin_manager
  USES_TERMINAL
  VERBATIM)
