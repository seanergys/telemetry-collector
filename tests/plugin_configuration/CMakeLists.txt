set(PLUGIN_CONFIGURATION_ARTIFACT_ROOT
    "${CMAKE_BINARY_DIR}/test-artifacts/plugin-configuration")
set(PLUGIN_CONFIGURATION_TEST_TARGETS)
set(PLUGIN_CONFIGURATION_TEST_WORKING_DIRECTORY
    "${CMAKE_BINARY_DIR}/tests/plugin_configuration")

file(MAKE_DIRECTORY
     "${PLUGIN_CONFIGURATION_TEST_WORKING_DIRECTORY}"
     "${PLUGIN_CONFIGURATION_ARTIFACT_ROOT}")

function(add_plugin_configuration_ctest target test_name gtest_filter)
  add_test(
    NAME "${test_name}"
    COMMAND "$<TARGET_FILE:${target}>" "--gtest_filter=${gtest_filter}")
  set_tests_properties(
    "${test_name}"
    PROPERTIES
      LABELS plugin_configuration
      WORKING_DIRECTORY "${PLUGIN_CONFIGURATION_TEST_WORKING_DIRECTORY}"
      ENVIRONMENT
      "PLUGIN_TEST_ARTIFACT_ROOT=${PLUGIN_CONFIGURATION_ARTIFACT_ROOT}")
endfunction()

add_executable(
  test_plugin_common_configuration
  test_plugin_configuration.cpp
  "${PROJECT_SOURCE_DIR}/src/config.cpp"
  "${PROJECT_SOURCE_DIR}/src/logger.cpp")
target_compile_features(test_plugin_common_configuration PRIVATE cxx_std_20)
target_compile_definitions(
  test_plugin_common_configuration
  PRIVATE
    PLUGIN_COMMON_UNIT_TESTS
    SPDLOG_FMT_EXTERNAL
    "TELEMETRY_COLLECTOR_MAIN_CONFIG=\"${CMAKE_BINARY_DIR}/config.yaml\""
    "PLUGIN_COMMON_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures/plugin_common\"")
target_include_directories(
  test_plugin_common_configuration
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
          "${PROJECT_SOURCE_DIR}/plugins")
target_link_libraries(
  test_plugin_common_configuration
  PRIVATE ${TELEMETRY_COLLECTOR_GTEST_TARGET}
          PkgConfig::SEANERBUS
          PkgConfig::UUID
          yaml-cpp::yaml-cpp
          spdlog::spdlog)

foreach(test_case IN ITEMS
        PluginCommonConfigurationTest.AppliesTemplateGroupAndSensorOverridesAndGeneratesMissingUuid
        PluginCommonConfigurationTest.MissingSensorListLeavesConfigsEmpty
        PluginCommonConfigurationTest.UnknownTemplateStillAppliesSensorFields
        PluginCommonConfigurationTest.AppliesSensorTemplateBeforeSensorOverrides
        PluginCommonConfigurationTest.MissingSensorGroupsLeavesConfigsEmpty
        PluginCommonConfigurationTest.AppliesUuidOnlyWhenValueIsValid
        PluginCommonConfigurationTest.RejectsNameServerUuidThatConflictsWithConfiguredUuid
        PluginCommonConfigurationTest.GeneratesDistinctUuidsForMissingSensorUuids
        PluginCommonConfigurationTest.AppliesLogLevelCaseInsensitively
        PluginCommonConfigurationTest.InvalidLogLevelKeepsExistingLevel
        PluginCommonConfigurationTest.InvalidIntervalTypeThrowsYamlConversionError
        PluginCommonConfigurationTest.EmptySensorListLeavesConfigsEmpty
        PluginCommonConfigurationTest.MainConfigEnablesPluginStatusLogging)
  add_plugin_configuration_ctest(
    test_plugin_common_configuration "${test_case}" "${test_case}")
endforeach()
list(APPEND PLUGIN_CONFIGURATION_TEST_TARGETS
     test_plugin_common_configuration)

foreach(plugin IN ITEMS mock snmp sysfs tester)
  if(TARGET "${plugin}")
    string(TOUPPER "${plugin}" plugin_upper)
    set(test_target "test_plugin_configuration_${plugin}")

    add_executable("${test_target}" test_plugin_configuration.cpp)
    target_compile_features("${test_target}" PRIVATE cxx_std_20)
    target_compile_definitions(
      "${test_target}"
      PRIVATE
        "PLUGIN_CONFIG_${plugin_upper}"
        SPDLOG_FMT_EXTERNAL
        "PLUGIN_LIBRARY_PATH=\"$<TARGET_FILE:${plugin}>\""
        "PLUGIN_CONFIG_FILE_PATH=\"${PROJECT_SOURCE_DIR}/config/${plugin}.yaml\"")
    target_include_directories(
      "${test_target}"
      PRIVATE "${PROJECT_SOURCE_DIR}/include"
              "${PROJECT_SOURCE_DIR}/plugins/${plugin}")
    target_link_libraries(
      "${test_target}"
      PRIVATE ${TELEMETRY_COLLECTOR_GTEST_TARGET}
              yaml-cpp::yaml-cpp
              spdlog::spdlog
              PkgConfig::SEANERBUS
              PkgConfig::UUID
              ${CMAKE_DL_LIBS})
    plugin_get_dependencies("${plugin}" plugin_dependencies)
    foreach(dep IN LISTS plugin_dependencies)
      target_link_libraries("${test_target}" PRIVATE "${dep}::${dep}")
    endforeach()
    add_dependencies("${test_target}" "${plugin}")
    if(plugin STREQUAL "mock")
      set(plugin_test_suite MockPluginConfigurationTest)
    elseif(plugin STREQUAL "snmp")
      set(plugin_test_suite SnmpPluginConfigurationTest)
    elseif(plugin STREQUAL "sysfs")
      set(plugin_test_suite SysfsPluginConfigurationTest)
    elseif(plugin STREQUAL "tester")
      set(plugin_test_suite TesterPluginConfigurationTest)
    endif()
    add_plugin_configuration_ctest(
      "${test_target}"
      "PluginConfiguration_${plugin}"
      "${plugin_test_suite}.ParsesYamlThroughConfigurator")
    if(plugin STREQUAL "snmp")
      add_plugin_configuration_ctest(
        "${test_target}"
        "PluginConfiguration_snmp_missing_required_fields"
        "SnmpPluginConfigurationTest.SkipsSensorsMissingRequiredOidOrConnection")
    elseif(plugin STREQUAL "sysfs")
      add_plugin_configuration_ctest(
        "${test_target}"
        "PluginConfiguration_sysfs_missing_path"
        "SysfsPluginConfigurationTest.SkipsSensorsWithMissingOrNonexistentPath")
    endif()
    list(APPEND PLUGIN_CONFIGURATION_TEST_TARGETS "${test_target}")
  endif()
endforeach()

add_custom_target(
  run_plugin_configuration_tests
  COMMAND
    "${CMAKE_COMMAND}"
    "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
    "-DARTIFACT_ROOT=${PLUGIN_CONFIGURATION_ARTIFACT_ROOT}"
    "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
    "-DREPORT_PYTHON=${Python3_EXECUTABLE}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_tests.cmake"
  DEPENDS ${PLUGIN_CONFIGURATION_TEST_TARGETS}
  USES_TERMINAL
  VERBATIM)
