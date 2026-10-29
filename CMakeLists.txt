cmake_minimum_required(VERSION 3.11)

project(
    telemetry_collector
    VERSION 0.1
    DESCRIPTION "SEANERGYS Telemetry Collector"
    LANGUAGES CXX
)

list(
    PREPEND
    CMAKE_MODULE_PATH
    "${PROJECT_SOURCE_DIR}/cmake"
)

# ============================================================================
# seanerbus / capnp
# ============================================================================

set(
    SEANERBUS_ROOT
    ""
    CACHE PATH
    "Root installation path of seanerbus"
)

set(
    CAPNP_ROOT
    ""
    CACHE PATH
    "Root installation path of capnp"
)

set(
    UUID_ROOT
    ""
    CACHE PATH
    "Root installation path of uuid"
)

find_package(
    PkgConfig
    REQUIRED
)

list(APPEND CMAKE_PREFIX_PATH "${SEANERBUS_ROOT}" "${CAPNP_ROOT}" "${UUID_ROOT}")

message(
    STATUS
    "PKG_CONFIG_PATH = $ENV{PKG_CONFIG_PATH}"
)

pkg_check_modules(
    SEANERBUS
    REQUIRED
    IMPORTED_TARGET
    seanerbus
)

pkg_check_modules(
    CAPNP 
    REQUIRED 
    IMPORTED_TARGET 
    capnp
)


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

set(
    MAIN_DEPENDENCIES
    yaml-cpp
    spdlog
)

# ============================================================================
# telemetry collector
# ============================================================================

file(
    GLOB
    TELEMETRY_COLLECTOR_SOURCES
    CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(
    telemetry_collector
    ${TELEMETRY_COLLECTOR_SOURCES}
)

target_compile_features(
    telemetry_collector
    PRIVATE
        cxx_std_20
)

target_compile_options(
    telemetry_collector
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Debug>:-O0;-g>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O2;-g>
)

target_include_directories(
    telemetry_collector
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)


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
#   - telemetry_collector
#   - selected plugins
#
# Duplicate dependencies are built only once.
# ============================================================================

resolve_dependencies(
    "${MAIN_DEPENDENCIES}"
    "${SELECTED_PLUGINS}"
)

# ============================================================================
# Main target dependencies
# ============================================================================

get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()


target_link_libraries(
    telemetry_collector
    PRIVATE
        PkgConfig::SEANERBUS
        yaml-cpp
	    spdlog::spdlog
        ${CMAKE_DL_LIBS}
)

set_target_properties(
    telemetry_collector
    PROPERTIES
        BUILD_RPATH "${SEANERBUS_LIBRARY_DIRS}"
)

target_compile_definitions(telemetry_collector PUBLIC SPDLOG_FMT_EXTERNAL)

target_link_libraries(telemetry_collector PRIVATE PkgConfig::UUID)

# ============================================================================
# Plugins
# ============================================================================

foreach(plugin IN LISTS SELECTED_PLUGINS)

    add_library(
        "${plugin}"
        SHARED
            "plugins/${plugin}/${plugin}_plugin.cpp"
            ${COMMON_SOURCES}
	    ${CMAKE_SOURCE_DIR}/src/logger.cpp # common functionalities between the TC and the plugins -> maybe separate this into common?
    )

    set_target_properties(
        "${plugin}"
        PROPERTIES
            PREFIX ""
    )

    target_compile_definitions("${plugin}" PUBLIC SPDLOG_FMT_EXTERNAL)
    
    target_include_directories(
        "${plugin}"
        PRIVATE BEFORE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/${plugin}"
            "${UUID_INCLUDEDIR}"
    )

    plugin_get_dependencies(
        "${plugin}"
        plugin_dependencies
    )

    target_link_libraries(
        "${plugin}"
        PRIVATE
	        yaml-cpp
            spdlog::spdlog
            PkgConfig::SEANERBUS
    )

    foreach(dep IN LISTS plugin_dependencies)
        target_link_libraries(
            "${plugin}"
            PRIVATE
                "${dep}::${dep}"
        )
    endforeach()

endforeach()

# ============================================================================
# Installation
# ============================================================================


include(GNUInstallDirs)

option(TC_CONFIG_INSTALL "Generate and install configs" OFF)
if(DEFINED CACHE{TC_CONFIG_INSTALL})
  set(TC_CONFIG_INSTALL ${TC_CONFIG_INSTALL}) 
  unset(TC_CONFIG_INSTALL CACHE) # prevent cache
else()
  set(TC_CONFIG_INSTALL OFF)
endif()

set(TC_CONFIG_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/telemetry_collector")
set(TC_PLUGIN_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/telemetry_collector/plugins")


install(
    TARGETS
        telemetry_collector
    RUNTIME DESTINATION
        ${CMAKE_INSTALL_BINDIR}
)

install(
    TARGETS
        ${SELECTED_PLUGINS}
    LIBRARY DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/telemetry_collector/plugins
)

if(TC_CONFIG_INSTALL)
    # Writes config.yaml into the build tree
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.yaml.in
                ${CMAKE_CURRENT_BINARY_DIR}/config.yaml @ONLY)


    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config/
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/telemetry_collector
            FILES_MATCHING PATTERN "*.yaml") # copy yamls to install

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config.yaml
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/telemetry_collector) # copy config to install

    message(STATUS "Config will be installed to: ${TC_CONFIG_FILE}")

else()
    message(STATUS "Config not installed (TC_CONFIG_INSTALL=OFF), using existing: ${TC_CONFIG_FILE}")
    install(CODE "message(STATUS \"Skipping config, existing: \$ENV{DESTDIR}${CMAKE_CURRENT_SOURCE_DIR}/config/\")")
endif()