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
)

# ============================================================================
# telemetry collector
# ============================================================================

add_executable(
    telemetry_collector
    src/telemetry-collector.cpp
    src/pluginManager.cpp
    src/config.cpp
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

if(NOT SEANERBUS_ROOT)

    message(FATAL_ERROR
        "SEANERBUS_ROOT must be specified, "
        "e.g. -DSEANERBUS_ROOT=/path/to/seanerbus/install"
    )

endif()

set(
    CAPNP_ROOT
    ""
    CACHE PATH
    "Root installation path of capnp"
)

if(NOT CAPNP_ROOT)

    message(FATAL_ERROR
        "CAPNP_ROOT must be specified, "
        "e.g. -DCAPNP_ROOT=/path/to/capnp/install"
    )

endif()

find_package(
    PkgConfig
    REQUIRED
)

set(
    ENV{PKG_CONFIG_PATH}
    "${SEANERBUS_ROOT}/lib64/pkgconfig:${CAPNP_ROOT}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}"
)

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
        yaml-cpp::yaml-cpp
        ${CMAKE_DL_LIBS}
)

set_target_properties(
    telemetry_collector
    PROPERTIES
        BUILD_RPATH "${SEANERBUS_LIBRARY_DIRS}"
)

# ============================================================================
# Plugins
# ============================================================================

foreach(plugin IN LISTS SELECTED_PLUGINS)

    add_library(
        "${plugin}"
        SHARED
            "plugins/${plugin}/${plugin}_plugin.cpp"
            ${COMMON_SOURCES}
    )

    set_target_properties(
        "${plugin}"
        PROPERTIES
            PREFIX ""
    )

    target_include_directories(
        "${plugin}"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/${plugin}"
    )

    plugin_get_dependencies(
        "${plugin}"
        plugin_dependencies
    )

    target_link_libraries(
        "${plugin}"
        PRIVATE
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
