include_guard(GLOBAL)

# ============================================================================
# Dependency registry
# ============================================================================

set(DEPENDENCIES
    net-snmp
    yaml-cpp
)

# ============================================================================
# net-snmp
# ============================================================================

dependency(
    NAME net-snmp
    VERSION 5.9.4
    GIT_REPOSITORY https://github.com/net-snmp/net-snmp.git
    GIT_TAG v5.9.4
    BUILD_TOOL autoconf
    CONFIGURE_ARGS
        --with-defaults
        --disable-agent
        --disable-manuals
        --disable-applications
    LIBRARIES netsnmp
)

# ============================================================================
# yaml-cpp
# ============================================================================

dependency(
    NAME yaml-cpp
    VERSION 0.8.0
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    BUILD_TOOL cmake
    LIBRARIES yaml-cpp
)
