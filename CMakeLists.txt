cmake_minimum_required(VERSION 3.11)

project(telemetry_collector VERSION 0.1 DESCRIPTION "SEANERGYS Telemetry Collector" LANGUAGES CXX)

list(PREPEND CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/cmake")


add_executable(telemetry_collector src/telemetry-collector.cpp
	src/pluginManager.cpp
	src/config.cpp)

target_include_directories(telemetry_collector PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_features(telemetry_collector PRIVATE cxx_std_20)

set_target_properties(telemetry_collector PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)

target_compile_options(telemetry_collector PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-O0;-g>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:RelWithDebInfo>:-O2;-g>
)

set(YAMLCPP_ROOT "" CACHE PATH "Root installation path of yaml-cpp")
set(SEANERBUS_ROOT "" CACHE PATH "Root installation path of seanerbus")
set(CAPNP_ROOT "" CACHE PATH "Root installation path of capnp")
message(STATUS "YAMLCPP_ROOT= ${YAMLCPP_ROOT}")
message(STATUS "SEANERBUS_ROOT= ${SEANERBUS_ROOT}")
message(STATUS "CAPNP_ROOT= ${CAPNP_ROOT}")

if(NOT YAMLCPP_ROOT)
    message(FATAL_ERROR "YAMLCPP_ROOT must be specified, e.g. -DYAMLCPP_ROOT=/path/to/yaml-cpp/install")
endif()

if(NOT SEANERBUS_ROOT)
    message(FATAL_ERROR "SEANERBUS_ROOT must be specified, e.g. -DSEANERBUS_ROOT=/path/to/seanerbus/install")
endif()

if(NOT CAPNP_ROOT)
	message(FATAL_ERROR "CAPNP_ROOT must be specified, e.g. -DCAPNP_ROOT=/path/to/capnp/install")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${YAMLCPP_ROOT}")
find_package(yaml-cpp REQUIRED)

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "${SEANERBUS_ROOT}/lib64/pkgconfig:$ENV{PKG_CONFIG_PATH}")

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "${CAPNP_ROOT}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
message(STATUS "PKG_CONFIG_PATH = $ENV{PKG_CONFIG_PATH}")

pkg_check_modules(SEANERBUS REQUIRED IMPORTED_TARGET seanerbus)

target_include_directories(telemetry_collector PRIVATE
    ${SEANERBUS_INCLUDE_DIRS}
)

target_include_directories(telemetry_collector PRIVATE
    ${YAML_CPP_INCLUDE_DIR}
)

target_link_directories(telemetry_collector PRIVATE ${SEANERBUS_LIBRARY_DIRS})

target_link_libraries(telemetry_collector PRIVATE 
	PkgConfig::SEANERBUS
	yaml-cpp::yaml-cpp
	${CMAKE_DL_LIBS}
	)

set_target_properties(telemetry_collector PROPERTIES
    BUILD_RPATH "${SEANERBUS_LIBRARY_DIRS}"
)

include(GNUInstallDirs)
install(TARGETS telemetry_collector RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
