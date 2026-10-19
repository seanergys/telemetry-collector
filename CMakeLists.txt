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
