# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: EUPL-1.2

cmake_minimum_required(VERSION 3.13)

project(telemetry_collector VERSION 0.6.0
        DESCRIPTION "SEANERGYS Telemetry Collector" LANGUAGES CXX)

include(CTest)

list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# ============================================================================
# seanerbus / capnp
# ============================================================================

set(SEANERBUS_ROOT "" CACHE PATH "Root installation path of seanerbus")

set(CAPNP_ROOT "" CACHE PATH "Root installation path of capnp")

find_package(PkgConfig REQUIRED)

list(APPEND CMAKE_PREFIX_PATH "${SEANERBUS_ROOT}" "${CAPNP_ROOT}")

message(STATUS "PKG_CONFIG_PATH = $ENV{PKG_CONFIG_PATH}")

pkg_check_modules(SEANERBUS REQUIRED IMPORTED_TARGET seanerbus)

pkg_check_modules(CAPNP REQUIRED IMPORTED_TARGET capnp)

pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)

# ============================================================================
# CMake modules
# ============================================================================

include(DependencyResolver)
include(Dependencies)
include(DependencyProviders)
include(PluginDependencies)
include(Plugins)
# ============================================================================
# Main dependency selection
# ============================================================================

set(MAIN_DEPENDENCIES yaml-cpp spdlog)

# ============================================================================
# telemetry collector
# ============================================================================

file(GLOB TELEMETRY_COLLECTOR_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(telemetry_collector ${TELEMETRY_COLLECTOR_SOURCES})

target_compile_features(telemetry_collector PRIVATE cxx_std_20)

target_compile_options(
  telemetry_collector
  PRIVATE -Wall
          -Wextra
          -Wpedantic
          $<$<CONFIG:Debug>:-O0;-g>
          $<$<CONFIG:Release>:-O3>
          $<$<CONFIG:RelWithDebInfo>:-O2;-g>)

target_include_directories(telemetry_collector
                           PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ============================================================================
# Plugins
# ============================================================================

discover_plugins()
select_plugins()

# ============================================================================
# Dependencies
#
# This resolves dependencies required by BOTH:
#
# * telemetry_collector
# * selected plugins
#
# Duplicate dependencies are built only once.
# ============================================================================

resolve_dependencies("${MAIN_DEPENDENCIES}" "${SELECTED_PLUGINS}")

# ============================================================================
# Main target dependencies
# ============================================================================

get_cmake_property(_variableNames VARIABLES)
list(SORT _variableNames)
foreach(_variableName ${_variableNames})
  message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

target_link_libraries(
  telemetry_collector PRIVATE PkgConfig::SEANERBUS yaml-cpp::yaml-cpp spdlog::spdlog
                              ${CMAKE_DL_LIBS})

set_target_properties(telemetry_collector
                      PROPERTIES
                        BUILD_RPATH "${SEANERBUS_LIBRARY_DIRS}"
                        INSTALL_RPATH_USE_LINK_PATH TRUE)

target_compile_definitions(telemetry_collector PUBLIC SPDLOG_FMT_EXTERNAL)


target_link_libraries(telemetry_collector PRIVATE PkgConfig::UUID)

# ============================================================================
# Plugins
# ============================================================================

foreach(plugin IN LISTS SELECTED_PLUGINS)

  add_library(
    "${plugin}" SHARED
    "plugins/${plugin}/${plugin}_plugin.cpp"
    ${COMMON_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/logger.cpp # common functionalities between the TC
                                       # and the plugins -> maybe separate this
                                       # into common?
  )

  set_target_properties(
    "${plugin}"
    PROPERTIES
      PREFIX ""
      INSTALL_RPATH_USE_LINK_PATH TRUE)

  target_compile_definitions("${plugin}" PUBLIC SPDLOG_FMT_EXTERNAL)

  target_include_directories(
    "${plugin}"
    PRIVATE BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/${plugin}"
            "${UUID_INCLUDEDIR}")

  plugin_get_dependencies("${plugin}" plugin_dependencies)

  target_link_libraries("${plugin}"
                        PRIVATE yaml-cpp::yaml-cpp
                                spdlog::spdlog
                                PkgConfig::SEANERBUS
                                PkgConfig::UUID)

  foreach(dep IN LISTS plugin_dependencies)
    target_link_libraries("${plugin}" PRIVATE "${dep}::${dep}")
  endforeach()

endforeach()

# ============================================================================
# Test groups
# ============================================================================

option(TC_BUILD_PLUGIN_MANAGER_TESTS "Build PluginManager tests" ON)
option(TC_BUILD_PLUGIN_CONFIGURATION_TESTS "Build plugin configuration tests" ON)
option(TC_BUILD_PLUGIN_TESTS "Build plugin tests" OFF)

if(BUILD_TESTING AND
   (TC_BUILD_PLUGIN_MANAGER_TESTS OR TC_BUILD_PLUGIN_CONFIGURATION_TESTS))
  find_package(GTest QUIET)

  if(GTest_FOUND)
    include(GoogleTest)

    if(TARGET GTest::gtest)
      set(TELEMETRY_COLLECTOR_GTEST_TARGET GTest::gtest)
    else()
      set(TELEMETRY_COLLECTOR_GTEST_TARGET GTest::GTest)
    endif()
  else()
    message(STATUS "GTest not found; GoogleTest groups will not be built")
  endif()
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================


include(GNUInstallDirs)

set(TC_CONFIG_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/telemetry_collector")
set(TC_PLUGIN_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/telemetry_collector/plugins")

option(TC_CONFIG_OVERWRITE "Overwrite existing configs during install" OFF)
if(DEFINED CACHE{TC_CONFIG_OVERWRITE})
  set(TC_CONFIG_OVERWRITE ${TC_CONFIG_OVERWRITE})
  unset(TC_CONFIG_OVERWRITE CACHE) # prevent cache
endif()

if(TC_CONFIG_OVERWRITE)
  set(_tc_override TRUE)
else()
  set(_tc_override FALSE)
endif()

install(TARGETS telemetry_collector RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS ${SELECTED_PLUGINS}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/telemetry_collector/plugins)

# Writes config.yaml to build
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.yaml.in
                ${CMAKE_CURRENT_BINARY_DIR}/config.yaml @ONLY)

# get all .yamls
file(GLOB _tc_plugin_yamls CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/config/*.yaml)
set(_tc_configs ${CMAKE_CURRENT_BINARY_DIR}/config.yaml ${_tc_plugin_yamls})

install(CODE "
  set(_override ${_tc_override})
  set(_dst \"\$ENV{DESTDIR}${TC_CONFIG_DIR}\")
  foreach(_src ${_tc_configs})
    get_filename_component(_name \"\${_src}\" NAME)
    if(NOT EXISTS \"\${_dst}/\${_name}\")
      message(STATUS \"New config: \${_dst}/\${_name}\")
      file(INSTALL \"\${_src}\" DESTINATION \"\${_dst}\")
    elseif(_override)
      message(STATUS \"Overriding config: \${_dst}/\${_name}\")
      file(INSTALL \"\${_src}\" DESTINATION \"\${_dst}\")
    else()
      message(STATUS \"Keeping existing config: \${_dst}/\${_name}\")
    endif()
  endforeach()
")
