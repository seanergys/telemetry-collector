#include "plugin.hpp"

#include <chrono>
#include <thread>

namespace
{
version_t* fixture_get_version()
{
    static version_t version{1, 0, 0};
    return &version;
}

void* fixture_create()
{
    return nullptr;
}

bool fixture_destroy(void*)
{
    return true;
}

bool fixture_init_sensors(void*)
{
    return true;
}

bool fixture_finit(void*)
{
    return true;
}

void* fixture_run(void*)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return nullptr;
}

void* fixture_configurator(void* config)
{
    return config;
}
}

#ifdef TEST_PLUGIN_MISSING_CREATE
#define FIXTURE_CREATE nullptr
#else
#define FIXTURE_CREATE fixture_create
#endif

#ifdef TEST_PLUGIN_MISSING_DESTROY
#define FIXTURE_DESTROY nullptr
#else
#define FIXTURE_DESTROY fixture_destroy
#endif

#ifdef TEST_PLUGIN_MISSING_INIT_SENSORS
#define FIXTURE_INIT_SENSORS nullptr
#else
#define FIXTURE_INIT_SENSORS fixture_init_sensors
#endif

#ifdef TEST_PLUGIN_MISSING_FINIT
#define FIXTURE_FINIT nullptr
#else
#define FIXTURE_FINIT fixture_finit
#endif

#ifdef TEST_PLUGIN_MISSING_RUN
#define FIXTURE_RUN nullptr
#else
#define FIXTURE_RUN fixture_run
#endif

extern "C"
{
plugin_interface_t plugin = {
    fixture_get_version,
    FIXTURE_CREATE,
    FIXTURE_DESTROY,
    FIXTURE_INIT_SENSORS,
    FIXTURE_FINIT,
    FIXTURE_RUN,
    fixture_configurator,
};
}
