find_package(Python3 3.6 QUIET COMPONENTS Interpreter)

if(TC_BUILD_PLUGIN_MANAGER_TESTS AND TELEMETRY_COLLECTOR_GTEST_TARGET)
  add_subdirectory(plugin_manager)
endif()

if(TC_BUILD_PLUGIN_CONFIGURATION_TESTS AND TELEMETRY_COLLECTOR_GTEST_TARGET)
  add_subdirectory(plugin_configuration)
endif()

if(TC_BUILD_PLUGIN_TESTS)
  add_subdirectory(plugin_tests)
endif()
