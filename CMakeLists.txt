cmake_minimum_required(VERSION 3.11)

project(telemetry_collector VERSION 0.1 DESCRIPTION "SEANERGYS Telemetry Collector" LANGUAGES CXX)

list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

add_executable(telemetry_collector src/telemetry-collector.cpp
	src/pluginManager.cpp
	src/config.cpp)

########### compile options
target_compile_features(telemetry_collector PRIVATE cxx_std_20)

target_compile_options(telemetry_collector PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-O0;-g>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O2;-g>
)
########### incldues
target_include_directories(telemetry_collector PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

########### yamlcpp
set(YAMLCPP_ROOT "" CACHE PATH "Root installation path of yaml-cpp")
if(YAMLCPP_ROOT)
    message(STATUS "Using installed yaml-cpp from ${YAMLCPP_ROOT}")
    list(PREPEND CMAKE_PREFIX_PATH "${YAMLCPP_ROOT}")
else()
    message(STATUS "YAMLCPP_ROOT not specified, fetching yaml-cpp")
    include(GitHubDependency)
    github_dependency(jbeder/yaml-cpp 0.8.0)
endif()


if(TARGET yaml-cpp)
	message(STATUS "SUCCESS: yaml-cpp target exists")
else()
	message(FATAL_ERROR "ERROR: yaml-cpp target does not exist")
endif()

########### seanerbus
set(SEANERBUS_ROOT "" CACHE PATH "Root installation path of seanerbus")
message(STATUS "SEANERBUS_ROOT= ${SEANERBUS_ROOT}")
if(NOT SEANERBUS_ROOT)
    message(FATAL_ERROR "SEANERBUS_ROOT must be specified, e.g. -DSEANERBUS_ROOT=/path/to/seanerbus/install")
endif()
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "${SEANERBUS_ROOT}/lib64/pkgconfig:$ENV{PKG_CONFIG_PATH}")

########### capnp
set(CAPNP_ROOT "" CACHE PATH "Root installation path of capnp")
message(STATUS "CAPNP_ROOT= ${CAPNP_ROOT}")
if(NOT CAPNP_ROOT)
	message(FATAL_ERROR "CAPNP_ROOT must be specified, e.g. -DCAPNP_ROOT=/path/to/capnp/install")
endif()
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "${CAPNP_ROOT}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
message(STATUS "PKG_CONFIG_PATH = $ENV{PKG_CONFIG_PATH}")

pkg_check_modules(SEANERBUS REQUIRED IMPORTED_TARGET seanerbus)

########### incldues
target_include_directories(telemetry_collector PRIVATE
    ${SEANERBUS_INCLUDE_DIRS}
)

target_include_directories(telemetry_collector PRIVATE
    ${YAML_CPP_INCLUDE_DIR}
)

########### linking
target_link_directories(telemetry_collector PRIVATE ${SEANERBUS_LIBRARY_DIRS})

target_link_libraries(telemetry_collector PRIVATE 
	PkgConfig::SEANERBUS
	yaml-cpp::yaml-cpp
	${CMAKE_DL_LIBS}
	)

set_target_properties(telemetry_collector PROPERTIES
    BUILD_RPATH "${SEANERBUS_LIBRARY_DIRS}"
)

########### installation
include(GNUInstallDirs)
install(TARGETS telemetry_collector RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
