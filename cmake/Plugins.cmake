# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: EUPL-1.2

include_guard(GLOBAL)

# ============================================================================
# discover_plugins()
# ============================================================================

function(discover_plugins)

  file(GLOB plugin_dirs CONFIGURE_DEPENDS LIST_DIRECTORIES true
       "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*")

  set(plugins "")

  foreach(dir IN LISTS plugin_dirs)

    if(IS_DIRECTORY "${dir}")

      get_filename_component(plugin "${dir}" NAME)

      list(APPEND plugins "${plugin}")

    endif()

  endforeach()

  list(SORT plugins)

  set(AVAILABLE_PLUGINS "${plugins}" PARENT_SCOPE)

  message(STATUS "Available plugins: ${plugins}")

endfunction()

# ============================================================================
# select_plugins()
# ============================================================================

function(select_plugins)

  if(NOT DEFINED SELECTED_PLUGINS)

    set(SELECTED_PLUGINS "${AVAILABLE_PLUGINS}" CACHE STRING "Plugins to build")

  endif()

  foreach(plugin IN LISTS SELECTED_PLUGINS)

    if(NOT plugin IN_LIST AVAILABLE_PLUGINS)

      message(FATAL_ERROR "Unknown plugin '${plugin}'. "
                          "Available: ${AVAILABLE_PLUGINS}")

    endif()

  endforeach()

  message(STATUS "Selected plugins: ${SELECTED_PLUGINS}")

endfunction()

# ============================================================================
# plugin_get_dependencies()
# ============================================================================

function(plugin_get_dependencies plugin output_variable)

  set(variable "PLUGIN_${plugin}_DEPENDENCIES")

  if(DEFINED ${variable})

    set(dependencies "${${variable}}")

  else()

    set(dependencies "")

  endif()

  set(${output_variable} "${dependencies}" PARENT_SCOPE)

endfunction()
