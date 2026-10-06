# ============================================================================
# DependencyResolver.cmake
# ============================================================================

include_guard(GLOBAL)

include(ExternalProject)
find_package(PkgConfig QUIET)
# ============================================================================
# Dependency build locations
# ============================================================================

set(DEPS_ROOT
    "${CMAKE_BINARY_DIR}/dependencies"
    CACHE PATH "Root directory for external dependencies"
)

set(DEPS_SOURCE_DIR
    "${DEPS_ROOT}/src"
)

set(DEPS_BUILD_DIR
    "${DEPS_ROOT}/build"
)

set(DEPS_INSTALL_DIR
    "${DEPS_ROOT}/install"
)

file(MAKE_DIRECTORY "${DEPS_SOURCE_DIR}" "${DEPS_BUILD_DIR}" "${DEPS_INSTALL_DIR}")

# ============================================================================
# dependency()
#
# Declare dependency metadata.
# ============================================================================

function(dependency)

    cmake_parse_arguments(
        DEP
        ""
        "NAME;VERSION;GIT_REPOSITORY;GIT_TAG;BUILD_TOOL"
        "CONFIGURE_ARGS;CMAKE_ARGS;LIBRARIES"
        ${ARGN}
    )

    if(NOT DEP_NAME)
        message(FATAL_ERROR "dependency() requires NAME")
    endif()

    set(DEPENDENCY_${DEP_NAME}_VERSION
        "${DEP_VERSION}"
        PARENT_SCOPE
    )

    set(DEPENDENCY_${DEP_NAME}_GIT_REPOSITORY
        "${DEP_GIT_REPOSITORY}"
        PARENT_SCOPE
    )

    set(DEPENDENCY_${DEP_NAME}_GIT_TAG
        "${DEP_GIT_TAG}"
        PARENT_SCOPE
    )

    set(DEPENDENCY_${DEP_NAME}_BUILD_TOOL
        "${DEP_BUILD_TOOL}"
        PARENT_SCOPE
    )

    set(DEPENDENCY_${DEP_NAME}_CONFIGURE_ARGS
        "${DEP_CONFIGURE_ARGS}"
        PARENT_SCOPE
    )

    set(
        DEPENDENCY_${DEP_NAME}_CMAKE_ARGS
        "${DEP_CMAKE_ARGS}"
        PARENT_SCOPE
    )

    set(
        DEPENDENCY_${DEP_NAME}_LIBRARIES
        "${DEP_LIBRARIES}"
        PARENT_SCOPE
    )

endfunction()

# ============================================================================
# dependency_get_libraries()
# ============================================================================

function(dependency_get_libraries dep output_variable)

    set(variable
        "DEPENDENCY_${dep}_LIBRARIES"
    )

    if(DEFINED ${variable})
        set(libraries
            "${${variable}}"
        )
    else()
        set(libraries
            ""
        )
    endif()

    set(${output_variable}
        "${libraries}"
        PARENT_SCOPE
    )

endfunction()

function(find_existing_dependency dep output_variable)

    set(package_name
        "${DEPENDENCY_PROVIDER_${dep}}"
    )

    if(NOT package_name)

        set(${output_variable}
            FALSE
            PARENT_SCOPE
        )

        return()

    endif()

    find_package("${package_name}" CONFIG QUIET)

    if(${package_name}_FOUND)

        message(STATUS "    using existing installation " "(${package_name})")

        set(${output_variable}
            TRUE
            PARENT_SCOPE
        )

    else()

        set(${output_variable}
            FALSE
            PARENT_SCOPE
        )

    endif()

endfunction()

# ============================================================================
# create_dependency_target()
#
# ExternalProject_Add() creates a UTILITY target, which cannot be linked.
#
# Therefore create:
#
# _dep_<name>       real INTERFACE target <name>::<name>    public ALIAS
#
# The interface target depends on the ExternalProject target.
# ============================================================================
function(create_dependency_target dep install_dir)

    set(target
        "_dep_${dep}"
    )

    if(TARGET "${target}")
        return()
    endif()

    add_library(
        "${target}"
        INTERFACE
    )

    add_library("${dep}::${dep}" ALIAS "${target}")

    add_dependencies("${target}" "${dep}")

    target_include_directories(
        "${target}"
        INTERFACE "${install_dir}/include"
    )

    dependency_get_libraries("${dep}" libraries)

    if(libraries)

        target_link_directories("${target}" INTERFACE "${install_dir}/lib" "${install_dir}/lib64")

        target_link_libraries(
            "${target}"
            INTERFACE ${libraries}
        )

    endif()

endfunction()

# ============================================================================
# build_dependency_autoconf()
# ============================================================================

function(build_dependency_autoconf dep)

    set(version
        "${DEPENDENCY_${dep}_VERSION}"
    )

    set(repository
        "${DEPENDENCY_${dep}_GIT_REPOSITORY}"
    )

    set(tag
        "${DEPENDENCY_${dep}_GIT_TAG}"
    )

    set(configure_args
        "${DEPENDENCY_${dep}_CONFIGURE_ARGS}"
    )

    set(install_dir
        "${DEPS_INSTALL_DIR}/${dep}"
    )

    message(STATUS "Configuring dependency ${dep} ${version} using Autoconf")

    ExternalProject_Add(
        "${dep}"
        PREFIX "${DEPS_ROOT}/${dep}"
        GIT_REPOSITORY "${repository}"
        GIT_TAG "${tag}"
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${install_dir} ${configure_args}
        BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -j
        INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    )

    create_dependency_target("${dep}" "${install_dir}")

endfunction()

# ============================================================================
# build_dependency_cmake()
# ============================================================================

function(build_dependency_cmake dep)

    set(version
        "${DEPENDENCY_${dep}_VERSION}"
    )

    set(repository
        "${DEPENDENCY_${dep}_GIT_REPOSITORY}"
    )

    set(tag
        "${DEPENDENCY_${dep}_GIT_TAG}"
    )

    set(
	    cmake_args
	    "${DEPENDENCY_${dep}_CMAKE_ARGS}"
    )

    set(
        install_dir
        "${DEPS_INSTALL_DIR}/${dep}"
    )

    message(STATUS "Configuring dependency ${dep} ${version} using CMake")

    ExternalProject_Add(
        "${dep}"

        PREFIX
            "${DEPS_ROOT}/${dep}"

        GIT_REPOSITORY
            "${repository}"

        GIT_TAG
            "${tag}"

        GIT_SHALLOW
            TRUE

        UPDATE_DISCONNECTED
            TRUE

        CMAKE_ARGS
    	    ${cmake_args}
            -DCMAKE_INSTALL_PREFIX=${install_dir}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}

        BUILD_COMMAND
            ${CMAKE_COMMAND}
            --build
            <BINARY_DIR>
            --parallel

        INSTALL_COMMAND
            ${CMAKE_COMMAND}
            --install
            <BINARY_DIR>
    )

    create_dependency_target("${dep}" "${install_dir}")

endfunction()

# ============================================================================
# build_dependency()
# ============================================================================

function(build_dependency dep)

    if(NOT dep IN_LIST DEPENDENCIES)
        message(FATAL_ERROR "Dependency '${dep}' is not listed in DEPENDENCIES.")
    endif()

    if(NOT DEFINED DEPENDENCY_${dep}_BUILD_TOOL)
        message(FATAL_ERROR "Dependency '${dep}' has no BUILD_TOOL.")
    endif()

    set(build_tool
        "${DEPENDENCY_${dep}_BUILD_TOOL}"
    )

    if(build_tool STREQUAL "autoconf")

        build_dependency_autoconf("${dep}")

    elseif(build_tool STREQUAL "cmake")

        build_dependency_cmake("${dep}")

    else()

        message(FATAL_ERROR "Unsupported BUILD_TOOL '${build_tool}' " "for dependency '${dep}'.")

    endif()

endfunction()

# ============================================================================
# resolve_dependencies()
#
# Resolve dependencies required by:
#
# 1. the main telemetry collector
# 2. selected plugins
#
# Dependencies are deduplicated and each dependency is added exactly once.
# ============================================================================

function(resolve_dependencies main_dependencies plugins)

    set(resolved_dependencies
        ${main_dependencies}
    )

    foreach(
        plugin IN
        LISTS plugins
    )

        plugin_get_dependencies("${plugin}" plugin_dependencies)

        list(APPEND resolved_dependencies ${plugin_dependencies})

    endforeach()

    list(REMOVE_DUPLICATES resolved_dependencies)

    message(STATUS "")
    message(STATUS "Dependency resolution")
    message(STATUS "---------------------")

    if(resolved_dependencies)

        foreach(
            dep IN
            LISTS resolved_dependencies
        )

            if(NOT dep IN_LIST DEPENDENCIES)

                message(FATAL_ERROR "Dependency '${dep}' is required, "
                                    "but is not listed in DEPENDENCIES."
                )

            endif()

            message(STATUS "  ${dep} " "(${DEPENDENCY_${dep}_VERSION})")

            find_existing_dependency("${dep}" dependency_found)

            if(NOT dependency_found)

                build_dependency("${dep}")

            endif()

        endforeach()

    else()

        message(STATUS "  No external dependencies required.")

    endif()

    message(STATUS "")

endfunction()
