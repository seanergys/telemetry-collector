#include "pluginManager.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

void snapshot_test_config(const std::string& filename) {
    const char* artifact_root = std::getenv("PLUGIN_TEST_ARTIFACT_ROOT");
    const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    if (!artifact_root || !test_info || !std::filesystem::exists(filename)) {
        return;
    }

    std::error_code error;
    const std::filesystem::path destination =
        std::filesystem::path(artifact_root) /
        (std::string(test_info->test_suite_name()) + "." + test_info->name()) /
        "configs" / filename;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) {
        ADD_FAILURE() << "Could not create test artifact directory: " << error.message();
        return;
    }

    std::filesystem::copy_file(
        filename,
        destination,
        std::filesystem::copy_options::overwrite_existing,
        error);
    if (error) {
        ADD_FAILURE() << "Could not save test config '" << filename
                      << "' to '" << destination << "': " << error.message();
    }
}

// Helper function to create a test config file
void create_test_config(const std::string& filename, const std::string& name, const std::string& uuid) {
    std::ofstream config_file(filename);
    config_file << "name: " << name << "\n";
    config_file << "uuid: " << uuid << "\n";
    config_file << "sensors:\n";
    config_file << "  - uuid: sensor-1\n";
    config_file << "    topic: test/topic\n";
    config_file << "    interval_ms: 1000\n";
    config_file << "    unit: test\n";
    config_file.close();
    snapshot_test_config(filename);
}

// Helper function to cleanup test files
void cleanup_test_file(const std::string& filename) {
    snapshot_test_config(filename);
    std::remove(filename.c_str());
}

#ifdef __APPLE__
    #define PLUGIN_EXT ".dylib"
#elif __linux__
    #define PLUGIN_EXT ".so"
#else
    #error "Unsupported platform - neither macOS nor Linux"
#endif

// Test fixture for PluginManager tests
class PluginManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup code if needed
    }

    void TearDown() override {
        // Common cleanup code if needed;
    }
};

class TestPluginManager : public PluginManager {
public:
    bool load_plugin(const char* path, const char* configFile, const char* uuid) {
        return PluginManager::load_plugin(path, configFile, uuid, "test://localhost");
    }
};

// ============================================================================
// TESTS FOR load_plugin() FUNCTION
// ============================================================================

// Test: Plugin with missing create function should be rejected during load
TEST_F(PluginManagerTest, 001_LoadPlugin_MissingCreate_ShouldBeRejected) {
    create_test_config("test_config_no_create.yaml", "NoCreatePlugin", "test001-no-create-123");

    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test_plugin_no_create" PLUGIN_EXT,
        "test_config_no_create.yaml",
        "test001-no-create-123");

    cleanup_test_file("test_config_no_create.yaml");

    ASSERT_FALSE(result) << "Plugin should be rejected for missing create function";
    ASSERT_FALSE(manager.is_plugin_loaded("test001-no-create-123")) << "Plugin should not be loaded";
}

// Test: Plugin with missing destroy function should be rejected during load
TEST_F(PluginManagerTest, 002_LoadPlugin_MissingDestroy_ShouldBeRejected) {
    create_test_config("test_config_no_destroy.yaml", "NoDestroyPlugin", "test002-no-destroy-456");

    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test_plugin_no_destroy" PLUGIN_EXT,
        "test_config_no_destroy.yaml",
        "test002-no-destroy-456");

    cleanup_test_file("test_config_no_destroy.yaml");

    ASSERT_FALSE(result) << "Plugin should be rejected for missing destroy function";
    ASSERT_FALSE(manager.is_plugin_loaded("test002-no-destroy-456")) << "Plugin should not be loaded";
}

// Test: Plugin with missing init_sensors should be rejected during load
TEST_F(PluginManagerTest, 003_LoadPlugin_MissingInitFunctions_ShouldBeRejected) {
    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test_plugin_no_init" PLUGIN_EXT,
        "test_config_no_init.yaml",
        "test003-no-init-789");

    cleanup_test_file("test_config_no_init.yaml");

    ASSERT_FALSE(result) << "Plugin should be rejected for missing init_sensors function";
    ASSERT_FALSE(manager.is_plugin_loaded("test003-no-init-789")) << "Plugin should not be loaded";
}

TEST_F(PluginManagerTest, 004_LoadPlugin_MissingFinit_ShouldBeRejected) {
    create_test_config("test_config_no_finit.yaml", "NoFinitPlugin", "test004-no-finit-012");

    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test_plugin_no_finit" PLUGIN_EXT,
        "test_config_no_finit.yaml",
        "test004-no-finit-012");

    cleanup_test_file("test_config_no_finit.yaml");

    ASSERT_FALSE(result) << "Plugin should be rejected for missing finit function";
    ASSERT_FALSE(manager.is_plugin_loaded("test004-no-finit-012")) << "Plugin should not be loaded";
}

// Test: Valid plugin with all required functions should load successfully
TEST_F(PluginManagerTest, 005_LoadPlugin_ValidPlugin_ShouldSucceed) {
    const char* plugin_path = "../plugins/test" PLUGIN_EXT;
    ASSERT_TRUE(std::filesystem::exists(plugin_path))
        << "Test plugin library should exist at " << plugin_path;
    create_test_config("test_config_valid_load.yaml", "ValidLoadPlugin", "test005-valid-load-345");

    TestPluginManager manager;
    bool result = manager.load_plugin(
        plugin_path,
        "test_config_valid_load.yaml",
        "test005-valid-load-345");

    cleanup_test_file("test_config_valid_load.yaml");

    ASSERT_TRUE(result) << "Valid plugin should load successfully";

    ASSERT_TRUE(manager.is_plugin_loaded("test005-valid-load-345")) << "Plugin should be loaded";
}

// Test: Plugin with missing both create and destroy should be rejected during load
TEST_F(PluginManagerTest, 006_LoadPlugin_MissingCreateAndDestroy_ShouldBeRejected) {
    create_test_config("test_config_no_create_destroy.yaml", "NoCreateDestroyPlugin", "test006-no-create-destroy-678");
    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test_plugin_no_create_destroy" PLUGIN_EXT,
        "test_config_no_create_destroy.yaml",
        "test006-no-create-destroy-678");

    cleanup_test_file("test_config_no_create_destroy.yaml");

    ASSERT_FALSE(result) << "Plugin should be rejected for missing create and destroy functions";
    ASSERT_FALSE(manager.is_plugin_loaded("test006-no-create-destroy-678")) << "Plugin should not be loaded";
}

// Test: Plugin with missing both init and finit should be rejected during load
TEST_F(PluginManagerTest, 007_LoadPlugin_MissingInitAndFinit_ShouldBeRejected) {
    create_test_config("test_config_no_init_finit.yaml", "NoInitFinitPlugin", "test007-no-init-finit-901");
    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test_plugin_no_init_finit" PLUGIN_EXT,
        "test_config_no_init_finit.yaml",
        "test007-no-init-finit-901");

    cleanup_test_file("test_config_no_init_finit.yaml");

    ASSERT_FALSE(result) << "Plugin should be rejected for missing init_sensors and finit functions";
    ASSERT_FALSE(manager.is_plugin_loaded("test007-no-init-finit-901")) << "Plugin should not be loaded";
}

// Test: Loading non-existent plugin file should fail
TEST_F(PluginManagerTest, 008_LoadPlugin_NonExistentFile_ShouldFail) {
    create_test_config("test_config_nonexistent.yaml", "NonExistentPlugin", "test008-nonexistent-234");
    const char* plugin_path = "../plugins/nonexistent_plugin" PLUGIN_EXT;
    ASSERT_FALSE(std::filesystem::exists(plugin_path))
        << "Test plugin path should not exist: " << plugin_path;

    TestPluginManager manager;
    bool result = manager.load_plugin(
        plugin_path,
        "test_config_nonexistent.yaml",
        "test008-nonexistent-234");
    cleanup_test_file("test_config_nonexistent.yaml");

    ASSERT_FALSE(result) << "Loading non-existent plugin file should fail";
    ASSERT_FALSE(manager.is_plugin_loaded("test008-nonexistent-234")) << "Plugin should not be loaded";
}

// Test: Loading plugin with invalid config file should fail
TEST_F(PluginManagerTest, 009_LoadPlugin_InvalidConfig_ShouldFail) {
    // Create an invalid YAML config file
    std::ofstream config_file("test_config_invalid.yaml");
    config_file << "invalid: yaml: content: [[[";
    config_file.close();
    ASSERT_TRUE(std::filesystem::exists("test_config_invalid.yaml"));

    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "test_config_invalid.yaml",
        "test009-invalid-config-567");

    cleanup_test_file("test_config_invalid.yaml");
    ASSERT_FALSE(result) << "Loading plugin with invalid config should fail";
    ASSERT_FALSE(manager.is_plugin_loaded("test009-invalid-config-567")) << "Plugin should not be loaded";
}

// Test: Loading plugin with missing config file should fail
TEST_F(PluginManagerTest, 010_LoadPlugin_MissingConfigFile_ShouldFail) {
    ASSERT_FALSE(std::filesystem::exists("nonexistent_config.yaml"));

    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "nonexistent_config.yaml",
        "test010-missing-config-890");

    ASSERT_FALSE(result) << "Loading plugin with missing config file should fail";
    ASSERT_FALSE(manager.is_plugin_loaded("test010-missing-config-890")) << "Plugin should not be loaded";
}

// Test: Loading same plugin twice with different UUIDs should succeed
TEST_F(PluginManagerTest, 011_LoadPlugin_SameLibraryDifferentUUIDs_ShouldSucceed) {
    create_test_config("test_config_uuid1.yaml", "Plugin1", "test011-uuid-1-a1b2c3");
    create_test_config("test_config_uuid2.yaml", "Plugin2", "test011-uuid-2-d4e5f6");
    TestPluginManager manager;
    bool result1 = manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "test_config_uuid1.yaml",
        "test011-uuid-1-a1b2c3");
    bool result2 = manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "test_config_uuid2.yaml",
        "test011-uuid-2-d4e5f6");

    cleanup_test_file("test_config_uuid1.yaml");
    cleanup_test_file("test_config_uuid2.yaml");

    ASSERT_TRUE(result1) << "First load should succeed";
    ASSERT_TRUE(result2) << "Second load should succeed";
    ASSERT_TRUE(manager.is_plugin_loaded("test011-uuid-1-a1b2c3")) << "First plugin should be loaded";
    ASSERT_TRUE(manager.is_plugin_loaded("test011-uuid-2-d4e5f6")) << "Second plugin should be loaded";
}

// Test: Loading plugin with same UUID twice should fail
TEST_F(PluginManagerTest, 012_LoadPlugin_SameUUID_ShouldFail) {
    create_test_config("test_config_same_uuid.yaml", "SameUUIDPlugin", "test012-same-uuid-789xyz");

    TestPluginManager manager;
    bool result1 = manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "test_config_same_uuid.yaml",
        "test012-same-uuid-789xyz");
    bool result2 = manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "test_config_same_uuid.yaml",
        "test012-same-uuid-789xyz");

    cleanup_test_file("test_config_same_uuid.yaml");

    ASSERT_TRUE(result1) << "First load should succeed";
    ASSERT_FALSE(result2) << "Second load should fail (plugin already loaded)";
    ASSERT_TRUE(manager.is_plugin_loaded("test012-same-uuid-789xyz")) << "Plugin should be loaded";
}
// Test: Plugin with sensors should initialize successfully
TEST_F(PluginManagerTest, 013_InitPlugin_WithSensors_ShouldSucceed) {
    create_test_config("test_config_with_sensors.yaml", "WithSensorsPlugin", "test013-with-sensors-abc123");

    TestPluginManager manager;
    bool load_result = manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "test_config_with_sensors.yaml",
        "test013-with-sensors-abc123");

    ASSERT_TRUE(load_result) << "Plugin should load successfully";

    bool init_result = manager.init_plugin("test013-with-sensors-abc123");

    // Clean up properly by finalizing the plugin
    bool finit_result = manager.finit_plugin("test013-with-sensors-abc123");

    cleanup_test_file("test_config_with_sensors.yaml");

    ASSERT_TRUE(init_result) << "Plugin with sensors should initialize successfully";
    ASSERT_TRUE(finit_result) << "Plugin should finalize successfully";
    ASSERT_TRUE(manager.is_plugin_loaded("test013-with-sensors-abc123")) << "Plugin should remain loaded";
}

// Test: init_plugin should fail for non-existent plugin
TEST_F(PluginManagerTest, 014_InitPlugin_NonExistentPlugin_ShouldFail) {
    TestPluginManager manager;
    bool result = manager.init_plugin("test014-non-existent-uuid-xyz789");

    ASSERT_FALSE(result) << "init_plugin should fail for non-existent plugin";
}

// Test: init_plugin should fail for plugin that is not loaded
TEST_F(PluginManagerTest, 015_InitPlugin_NotLoaded_ShouldFail) {
    TestPluginManager manager;
    // Don't load the plugin, just try to init
    bool result = manager.init_plugin("test015-not-loaded-uuid-abc456");

    ASSERT_FALSE(result) << "init_plugin should fail for plugin that is not loaded";
}

TEST_F(PluginManagerTest, 016_InitPlugin_AlreadyInitialized_ShouldFail)
{
    create_test_config("test_config_init_twice.yaml", "InitTwicePlugin", "test016-init-twice-uuid-def789");
    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_init_twice.yaml", "test016-init-twice-uuid-def789");
    bool result1 = manager.init_plugin("test016-init-twice-uuid-def789");
    bool result2 = manager.init_plugin("test016-init-twice-uuid-def789"); // Init again

    // Clean up properly by finalizing and unloading the plugin
    bool finit_result = manager.finit_plugin("test016-init-twice-uuid-def789");
    bool unload_result = manager.unload_plugin("test016-init-twice-uuid-def789");

    cleanup_test_file("test_config_init_twice.yaml");

    ASSERT_TRUE(result1) << "First init should succeed";
    ASSERT_FALSE(result2) << "Second init should fail (plugin already initialized)";
    ASSERT_TRUE(finit_result) << "finit_plugin should succeed";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
    ASSERT_FALSE(manager.is_plugin_loaded("test016-init-twice-uuid-def789")) << "Plugin should be unloaded";
}

// Test: init_plugin should fail for a running plugin
TEST_F(PluginManagerTest, 017_InitPlugin_RunningPlugin_ShouldFail) {
    create_test_config("test_config_init_running.yaml", "InitRunningPlugin", "test016b-init-running-uuid-def789");
    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_init_running.yaml", "test016b-init-running-uuid-def789");
    manager.init_plugin("test016b-init-running-uuid-def789");
    manager.start_plugin("test016b-init-running-uuid-def789");

    bool result = manager.init_plugin("test016b-init-running-uuid-def789"); // Try to init again while running

    cleanup_test_file("test_config_init_running.yaml");

    ASSERT_FALSE(result) << "init_plugin should fail for running plugin";
    ASSERT_TRUE(manager.is_plugin_loaded("test016b-init-running-uuid-def789")) << "Plugin should still be loaded";

    ASSERT_TRUE(manager.stop_plugin("test016b-init-running-uuid-def789"));
    ASSERT_TRUE(manager.finit_plugin("test016b-init-running-uuid-def789"));
}

// ============================================================================
// TESTS FOR finit_plugin() FUNCTION
// ============================================================================

// Test: finit_plugin should succeed for a loaded and initialized plugin
TEST_F(PluginManagerTest, 018_FinitPlugin_ValidPlugin_ShouldSucceed) {
    create_test_config("test_config_finit.yaml", "FinitPlugin", "test017-finit-uuid-ghi012");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_finit.yaml", "test017-finit-uuid-ghi012");
    manager.init_plugin("test017-finit-uuid-ghi012");
    bool finit_result = manager.finit_plugin("test017-finit-uuid-ghi012");

    // Clean up properly by unloading the plugin
    bool unload_result = manager.unload_plugin("test017-finit-uuid-ghi012");

    cleanup_test_file("test_config_finit.yaml");

    ASSERT_TRUE(finit_result) << "finit_plugin should succeed for valid plugin";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed after finit";
    ASSERT_FALSE(manager.is_plugin_loaded("test017-finit-uuid-ghi012")) << "Plugin should be unloaded";
}

// Test: finit_plugin should fail when called on already finited plugin
TEST_F(PluginManagerTest, 019_FinitPlugin_AlreadyFinit_ShouldFail) {
    create_test_config("test_config_finit_twice.yaml", "FinitTwicePlugin", "test017b-finit-twice-uuid-jkl345");
    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_finit_twice.yaml", "test017b-finit-twice-uuid-jkl345");
    manager.init_plugin("test017b-finit-twice-uuid-jkl345");
    manager.start_plugin("test017b-finit-twice-uuid-jkl345");
    manager.stop_plugin("test017b-finit-twice-uuid-jkl345");
    bool result1 = manager.finit_plugin("test017b-finit-twice-uuid-jkl345");
    bool result2 = manager.finit_plugin("test017b-finit-twice-uuid-jkl345"); // Finit again

    cleanup_test_file("test_config_finit_twice.yaml");

    ASSERT_TRUE(result1) << "First finit should succeed";
    ASSERT_FALSE(result2) << "Second finit should fail (plugin already finited)";
}

// Test: finit_plugin should fail for non-existent plugin
TEST_F(PluginManagerTest, 020_FinitPlugin_NonExistentPlugin_ShouldFail) {
    TestPluginManager manager;
    bool result = manager.finit_plugin("test018-non-existent-uuid-jkl345");

    ASSERT_FALSE(result) << "finit_plugin should fail for non-existent plugin";
}

// ============================================================================
// TESTS FOR start_plugin() FUNCTION
// ============================================================================

// Test: start_plugin should succeed for an initialized plugin
TEST_F(PluginManagerTest, 021_StartPlugin_ValidPlugin_ShouldSucceed) {
    create_test_config("test_config_start.yaml", "StartPlugin", "test019-start-uuid-mno678");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_start.yaml", "test019-start-uuid-mno678");
    manager.init_plugin("test019-start-uuid-mno678");

    bool start_result = manager.start_plugin("test019-start-uuid-mno678");

    // Clean up properly
    bool stop_result = manager.stop_plugin("test019-start-uuid-mno678");
    bool finit_result = manager.finit_plugin("test019-start-uuid-mno678");
    bool unload_result = manager.unload_plugin("test019-start-uuid-mno678");

    cleanup_test_file("test_config_start.yaml");

    ASSERT_TRUE(start_result) << "start_plugin should succeed for valid plugin";
    ASSERT_TRUE(stop_result) << "stop_plugin should succeed";
    ASSERT_TRUE(finit_result) << "finit_plugin should succeed";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: start_plugin should fail for non-existent plugin
TEST_F(PluginManagerTest, 022_StartPlugin_NonExistentPlugin_ShouldFail) {
    TestPluginManager manager;
    bool result = manager.start_plugin("test020-non-existent-uuid-pqr901");

    ASSERT_FALSE(result) << "start_plugin should fail for non-existent plugin";
}

// Test: start_plugin should fail for plugin that is not initialized
TEST_F(PluginManagerTest, 023_StartPlugin_NotInitialized_ShouldFail) {
    create_test_config("test_config_start_not_init.yaml", "StartNotInitPlugin", "test021-start-not-init-uuid-stu234");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_start_not_init.yaml", "test021-start-not-init-uuid-stu234");
    // Don't initialize, just try to start
    bool result = manager.start_plugin("test021-start-not-init-uuid-stu234");


    cleanup_test_file("test_config_start_not_init.yaml");

    // Should fail because plugin is not initialized
    ASSERT_FALSE(result) << "start_plugin should fail for plugin that is not initialized";
    ASSERT_TRUE(manager.is_plugin_loaded("test021-start-not-init-uuid-stu234")) << "Plugin should still be loaded";
}

// Test: start_plugin should fail when called on already started plugin
TEST_F(PluginManagerTest, 024_StartPlugin_AlreadyStarted_ShouldFail)
{
    create_test_config("test_config_start_twice.yaml", "StartTwicePlugin", "test022-start-twice-uuid-vwx567");
    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_start_twice.yaml", "test022-start-twice-uuid-vwx567");
    manager.init_plugin("test022-start-twice-uuid-vwx567");
    bool result1 = manager.start_plugin("test022-start-twice-uuid-vwx567");
    bool result2 = manager.start_plugin("test022-start-twice-uuid-vwx567"); // Start again

    cleanup_test_file("test_config_start_twice.yaml");

    ASSERT_TRUE(result1) << "First start should succeed";
    ASSERT_FALSE(result2) << "Second start should fail (plugin already started)";

    ASSERT_TRUE(manager.stop_plugin("test022-start-twice-uuid-vwx567"));
    ASSERT_TRUE(manager.finit_plugin("test022-start-twice-uuid-vwx567"));
    ASSERT_TRUE(manager.unload_plugin("test022-start-twice-uuid-vwx567"));
}

// Test: start_plugin should fail for an unloaded plugin
TEST_F(PluginManagerTest, 025_StartPlugin_UnloadedPlugin_ShouldFail) {
    create_test_config("test_config_start_unloaded.yaml", "StartUnloadedPlugin", "test022b-start-unloaded-uuid-vwx567");
    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_start_unloaded.yaml", "test022b-start-unloaded-uuid-vwx567");
    manager.unload_plugin("test022b-start-unloaded-uuid-vwx567");

    bool result = manager.start_plugin("test022b-start-unloaded-uuid-vwx567"); // Try to start unloaded plugin

    cleanup_test_file("test_config_start_unloaded.yaml");

    ASSERT_FALSE(result) << "start_plugin should fail for unloaded plugin";
    ASSERT_FALSE(manager.is_plugin_loaded("test022b-start-unloaded-uuid-vwx567")) << "Plugin should remain unloaded";
    ASSERT_FALSE(manager.unload_plugin("test022b-start-unloaded-uuid-vwx567"));
}

// ============================================================================
// TESTS FOR stop_plugin() FUNCTION
// ============================================================================

// Test: stop_plugin should succeed for a running plugin
TEST_F(PluginManagerTest, 026_StopPlugin_RunningPlugin_ShouldSucceed) {
    create_test_config("test_config_stop.yaml", "StopPlugin", "test023-stop-uuid-yza890");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_stop.yaml", "test023-stop-uuid-yza890");
    manager.init_plugin("test023-stop-uuid-yza890");
    manager.start_plugin("test023-stop-uuid-yza890");

    bool stop_result = manager.stop_plugin("test023-stop-uuid-yza890");

    // Clean up properly
    bool finit_result = manager.finit_plugin("test023-stop-uuid-yza890");
    bool unload_result = manager.unload_plugin("test023-stop-uuid-yza890");

    cleanup_test_file("test_config_stop.yaml");

    ASSERT_TRUE(stop_result) << "stop_plugin should succeed for running plugin";
    ASSERT_TRUE(finit_result) << "finit_plugin should succeed";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: stop_plugin should fail for non-existent plugin
TEST_F(PluginManagerTest, 027_StopPlugin_NonExistentPlugin_ShouldFail) {
    TestPluginManager manager;
    bool result = manager.stop_plugin("test024-non-existent-uuid-bcd123");

    ASSERT_FALSE(result) << "stop_plugin should fail for non-existent plugin";
}

// Test: stop_plugin should fail when called on plugin that is not running
TEST_F(PluginManagerTest, 028_StopPlugin_NotRunning_ShouldFail) {
    create_test_config("test_config_stop_not_running.yaml", "StopNotRunningPlugin", "test025-stop-not-running-uuid-efg456");
    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_stop_not_running.yaml", "test025-stop-not-running-uuid-efg456");
    manager.init_plugin("test025-stop-not-running-uuid-efg456");
    // Don't start, just try to stop
    bool result = manager.stop_plugin("test025-stop-not-running-uuid-efg456");

    cleanup_test_file("test_config_stop_not_running.yaml");

    ASSERT_FALSE(result) << "stop_plugin should fail for plugin that is not running";

    manager.finit_plugin("test025-stop-not-running-uuid-efg456");
}

// Test: stop_plugin should fail when called on already stopped plugin
TEST_F(PluginManagerTest, 029_StopPlugin_AlreadyStopped_ShouldFail) {
    create_test_config("test_config_stop_twice.yaml", "StopTwicePlugin", "test026-stop-twice-uuid-hij789");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_stop_twice.yaml", "test026-stop-twice-uuid-hij789");
    manager.init_plugin("test026-stop-twice-uuid-hij789");
    manager.start_plugin("test026-stop-twice-uuid-hij789");
    bool result1 = manager.stop_plugin("test026-stop-twice-uuid-hij789");
    bool result2 = manager.stop_plugin("test026-stop-twice-uuid-hij789"); // Stop again

    // Clean up properly
    bool finit_result = manager.finit_plugin("test026-stop-twice-uuid-hij789");
    bool unload_result = manager.unload_plugin("test026-stop-twice-uuid-hij789");

    cleanup_test_file("test_config_stop_twice.yaml");

    ASSERT_TRUE(result1) << "First stop should succeed";
    ASSERT_FALSE(result2) << "Second stop should fail (plugin already stopped)";
    ASSERT_TRUE(finit_result) << "finit_plugin should succeed";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// ============================================================================
// TESTS FOR unload_plugin() FUNCTION
// ============================================================================

// Test: unload_plugin should succeed for a loaded plugin
TEST_F(PluginManagerTest, 030_UnloadPlugin_ValidPlugin_ShouldSucceed) {
    create_test_config("test_config_unload.yaml", "UnloadPlugin", "test027-unload-uuid-klm012");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_unload.yaml", "test027-unload-uuid-klm012");

    bool unload_result = manager.unload_plugin("test027-unload-uuid-klm012");

    cleanup_test_file("test_config_unload.yaml");

    ASSERT_TRUE(unload_result) << "unload_plugin should succeed for valid plugin";
    ASSERT_FALSE(manager.is_plugin_loaded("test027-unload-uuid-klm012")) << "Plugin should be unloaded";
}

// Test: unload_plugin should fail for non-existent plugin
TEST_F(PluginManagerTest, 031_UnloadPlugin_NonExistentPlugin_ShouldFail) {
    TestPluginManager manager;
    bool result = manager.unload_plugin("test028-non-existent-uuid-nop012");

    ASSERT_FALSE(result) << "unload_plugin should fail for non-existent plugin";
}

// Test: unload_plugin should fail when called on already unloaded plugin
TEST_F(PluginManagerTest, 032_UnloadPlugin_AlreadyUnloaded_ShouldFail) {
    create_test_config("test_config_unload_twice.yaml", "UnloadTwicePlugin", "test029-unload-twice-uuid-qrs345");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_unload_twice.yaml", "test029-unload-twice-uuid-qrs345");
    manager.init_plugin("test029-unload-twice-uuid-qrs345");
    manager.start_plugin("test029-unload-twice-uuid-qrs345");
    manager.stop_plugin("test029-unload-twice-uuid-qrs345");
    manager.finit_plugin("test029-unload-twice-uuid-qrs345");
    bool result1 = manager.unload_plugin("test029-unload-twice-uuid-qrs345");
    bool result2 = manager.unload_plugin("test029-unload-twice-uuid-qrs345"); // Unload again
    cleanup_test_file("test_config_unload_twice.yaml");

    ASSERT_TRUE(result1) << "First unload should succeed";
    ASSERT_FALSE(result2) << "Second unload should fail (plugin already unloaded)";
    ASSERT_FALSE(manager.is_plugin_loaded("test029-unload-twice-uuid-qrs345")) << "Plugin should be unloaded";
}

// Test: unload_plugin should fail for a running plugin
TEST_F(PluginManagerTest, 033_UnloadPlugin_RunningPlugin_ShouldFail)
{
    create_test_config("test_config_unload_running.yaml", "UnloadRunningPlugin", "test030-unload-running-uuid-tuv678");
    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_unload_running.yaml", "test030-unload-running-uuid-tuv678");
    manager.init_plugin("test030-unload-running-uuid-tuv678");
    manager.start_plugin("test030-unload-running-uuid-tuv678");

    bool unload_result = manager.unload_plugin("test030-unload-running-uuid-tuv678");

    cleanup_test_file("test_config_unload_running.yaml");

    ASSERT_FALSE(unload_result) << "unload_plugin should fail for running plugin";
    ASSERT_TRUE(manager.is_plugin_loaded("test030-unload-running-uuid-tuv678")) << "Plugin should still be loaded";

    manager.stop_plugin("test030-unload-running-uuid-tuv678");
    manager.finit_plugin("test030-unload-running-uuid-tuv678");
}

// Test: unload_plugin should fail for an initialized plugin
TEST_F(PluginManagerTest, 034_UnloadPlugin_InitializedPlugin_ShouldFail) {
    create_test_config("test_config_unload_init.yaml", "UnloadInitPlugin", "test030b-unload-init-uuid-xyz123");
    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_unload_init.yaml", "test030b-unload-init-uuid-xyz123");
    manager.init_plugin("test030b-unload-init-uuid-xyz123");

    bool unload_result = manager.unload_plugin("test030b-unload-init-uuid-xyz123");

    cleanup_test_file("test_config_unload_init.yaml");

    ASSERT_FALSE(unload_result) << "unload_plugin should fail for initialized plugin";
    ASSERT_TRUE(manager.is_plugin_loaded("test030b-unload-init-uuid-xyz123")) << "Plugin should still be loaded";

    manager.finit_plugin("test030b-unload-init-uuid-xyz123");
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

// Test: Complete lifecycle - load, init, start, stop, finit, unload
TEST_F(PluginManagerTest, 035_CompleteLifecycle_ShouldSucceed) {
    create_test_config("test_config_lifecycle.yaml", "LifecyclePlugin", "test031-lifecycle-uuid-wxy901");

    TestPluginManager manager;
    // Load
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_lifecycle.yaml", "test031-lifecycle-uuid-wxy901"))
        << "Load should succeed";
    ASSERT_TRUE(manager.is_plugin_loaded("test031-lifecycle-uuid-wxy901")) << "Plugin should be loaded";

    // Init
    ASSERT_TRUE(manager.init_plugin("test031-lifecycle-uuid-wxy901")) << "Init should succeed";

    // Start
    ASSERT_TRUE(manager.start_plugin("test031-lifecycle-uuid-wxy901")) << "Start should succeed";

    // Stop
    ASSERT_TRUE(manager.stop_plugin("test031-lifecycle-uuid-wxy901")) << "Stop should succeed";

    // Finit
    ASSERT_TRUE(manager.finit_plugin("test031-lifecycle-uuid-wxy901")) << "Finit should succeed";

    // Unload
    ASSERT_TRUE(manager.unload_plugin("test031-lifecycle-uuid-wxy901")) << "Unload should succeed";
    ASSERT_FALSE(manager.is_plugin_loaded("test031-lifecycle-uuid-wxy901")) << "Plugin should be unloaded";

    cleanup_test_file("test_config_lifecycle.yaml");
}

// Test: Load 5 plugins, initialize 3, start 2, stop 1, finit 2, unload 4
TEST_F(PluginManagerTest, 036_MultiplePlugins_MixedOperations_ShouldSucceed) {
    // Create configs for 5 plugins
    create_test_config("test_config_multi_1.yaml", "MultiPlugin1", "test032-alpha-bravo-001-xyz");
    create_test_config("test_config_multi_2.yaml", "MultiPlugin2", "test032-charlie-delta-002-abc");
    create_test_config("test_config_multi_3.yaml", "MultiPlugin3", "test032-echo-foxtrot-003-def");
    create_test_config("test_config_multi_4.yaml", "MultiPlugin4", "test032-golf-hotel-004-ghi");
    create_test_config("test_config_multi_5.yaml", "MultiPlugin5", "test032-india-juliet-005-jkl");

    TestPluginManager manager;

    // Load all 5 plugins
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_multi_1.yaml", "test032-alpha-bravo-001-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_multi_2.yaml", "test032-charlie-delta-002-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_multi_3.yaml", "test032-echo-foxtrot-003-def"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_multi_4.yaml", "test032-golf-hotel-004-ghi"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_multi_5.yaml", "test032-india-juliet-005-jkl"));

    ASSERT_TRUE(manager.is_plugin_loaded("test032-alpha-bravo-001-xyz"));
    ASSERT_TRUE(manager.is_plugin_loaded("test032-charlie-delta-002-abc"));
    ASSERT_TRUE(manager.is_plugin_loaded("test032-echo-foxtrot-003-def"));
    ASSERT_TRUE(manager.is_plugin_loaded("test032-golf-hotel-004-ghi"));
    ASSERT_TRUE(manager.is_plugin_loaded("test032-india-juliet-005-jkl"));

    // Initialize only 3 plugins (1, 3, 5)
    ASSERT_TRUE(manager.init_plugin("test032-alpha-bravo-001-xyz"));
    ASSERT_TRUE(manager.init_plugin("test032-echo-foxtrot-003-def"));
    ASSERT_TRUE(manager.init_plugin("test032-india-juliet-005-jkl"));

    // Start only 2 plugins (1 and 5)
    ASSERT_TRUE(manager.start_plugin("test032-alpha-bravo-001-xyz"));
    ASSERT_TRUE(manager.start_plugin("test032-india-juliet-005-jkl"));

    // Stop only 1 plugin (1)
    ASSERT_TRUE(manager.stop_plugin("test032-alpha-bravo-001-xyz"));

    // Finit 2 plugins (3 and 5)
    ASSERT_TRUE(manager.finit_plugin("test032-echo-foxtrot-003-def"));
    ASSERT_FALSE(manager.finit_plugin("test032-india-juliet-005-jkl"));

    // Stop and finit plugins 1 before unloading
    ASSERT_FALSE(manager.stop_plugin("test032-alpha-bravo-001-xyz"));
    ASSERT_TRUE(manager.finit_plugin("test032-alpha-bravo-001-xyz"));

    ASSERT_TRUE(manager.stop_plugin("test032-india-juliet-005-jkl"));
    ASSERT_TRUE(manager.finit_plugin("test032-india-juliet-005-jkl"));


    // Unload 4 plugins (1, 2, 3, 4), leaving only 5
    ASSERT_TRUE(manager.unload_plugin("test032-alpha-bravo-001-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test032-charlie-delta-002-abc"));
    ASSERT_TRUE(manager.unload_plugin("test032-echo-foxtrot-003-def"));
    ASSERT_TRUE(manager.unload_plugin("test032-golf-hotel-004-ghi"));

    // Verify final state
    ASSERT_FALSE(manager.is_plugin_loaded("test032-alpha-bravo-001-xyz"));
    ASSERT_FALSE(manager.is_plugin_loaded("test032-charlie-delta-002-abc"));
    ASSERT_FALSE(manager.is_plugin_loaded("test032-echo-foxtrot-003-def"));
    ASSERT_FALSE(manager.is_plugin_loaded("test032-golf-hotel-004-ghi"));
    ASSERT_TRUE(manager.is_plugin_loaded("test032-india-juliet-005-jkl")) << "Plugin 5 should still be loaded";

    // Cleanup
    cleanup_test_file("test_config_multi_1.yaml");
    cleanup_test_file("test_config_multi_2.yaml");
    cleanup_test_file("test_config_multi_3.yaml");
    cleanup_test_file("test_config_multi_4.yaml");
    cleanup_test_file("test_config_multi_5.yaml");

    manager.unload_plugin("test032-india-juliet-005-jkl");

}

// Test: Load 3 plugins, start all, stop in reverse order, unload all
TEST_F(PluginManagerTest, 037_MultiplePlugins_StartAllStopReverse_ShouldSucceed) {
    create_test_config("test_config_rev_1.yaml", "ReversePlugin1", "test033-kilo-lima-101-xyz");
    create_test_config("test_config_rev_2.yaml", "ReversePlugin2", "test033-mike-november-102-abc");
    create_test_config("test_config_rev_3.yaml", "ReversePlugin3", "test033-oscar-papa-103-def");

    TestPluginManager manager;

    // Load all
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rev_1.yaml", "test033-kilo-lima-101-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rev_2.yaml", "test033-mike-november-102-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rev_3.yaml", "test033-oscar-papa-103-def"));

    // Init all
    ASSERT_TRUE(manager.init_plugin("test033-kilo-lima-101-xyz"));
    ASSERT_TRUE(manager.init_plugin("test033-mike-november-102-abc"));
    ASSERT_TRUE(manager.init_plugin("test033-oscar-papa-103-def"));

    // Start all
    ASSERT_TRUE(manager.start_plugin("test033-kilo-lima-101-xyz"));
    ASSERT_TRUE(manager.start_plugin("test033-mike-november-102-abc"));
    ASSERT_TRUE(manager.start_plugin("test033-oscar-papa-103-def"));

    // Stop in reverse order
    ASSERT_TRUE(manager.stop_plugin("test033-oscar-papa-103-def"));
    ASSERT_TRUE(manager.stop_plugin("test033-mike-november-102-abc"));
    ASSERT_TRUE(manager.stop_plugin("test033-kilo-lima-101-xyz"));

    // Finit all
    ASSERT_TRUE(manager.finit_plugin("test033-oscar-papa-103-def"));
    ASSERT_TRUE(manager.finit_plugin("test033-mike-november-102-abc"));
    ASSERT_TRUE(manager.finit_plugin("test033-kilo-lima-101-xyz"));

    // Unload all
    ASSERT_TRUE(manager.unload_plugin("test033-kilo-lima-101-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test033-mike-november-102-abc"));
    ASSERT_TRUE(manager.unload_plugin("test033-oscar-papa-103-def"));

    ASSERT_FALSE(manager.is_plugin_loaded("test033-kilo-lima-101-xyz"));
    ASSERT_FALSE(manager.is_plugin_loaded("test033-mike-november-102-abc"));
    ASSERT_FALSE(manager.is_plugin_loaded("test033-oscar-papa-103-def"));

    cleanup_test_file("test_config_rev_1.yaml");
    cleanup_test_file("test_config_rev_2.yaml");
    cleanup_test_file("test_config_rev_3.yaml");
}

// Test: Load 4 plugins, randomly unload some, then operate on remaining
TEST_F(PluginManagerTest, 038_MultiplePlugins_RandomUnloadThenOperate_ShouldSucceed) {
    create_test_config("test_config_rand_1.yaml", "RandomPlugin1", "test034-quebec-romeo-201-xyz");
    create_test_config("test_config_rand_2.yaml", "RandomPlugin2", "test034-sierra-tango-202-abc");
    create_test_config("test_config_rand_3.yaml", "RandomPlugin3", "test034-uniform-victor-203-def");
    create_test_config("test_config_rand_4.yaml", "RandomPlugin4", "test034-whiskey-xray-204-ghi");

    TestPluginManager manager;

    // Load all 4
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rand_1.yaml", "test034-quebec-romeo-201-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rand_2.yaml", "test034-sierra-tango-202-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rand_3.yaml", "test034-uniform-victor-203-def"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rand_4.yaml", "test034-whiskey-xray-204-ghi"));

    // Unload plugin 2 immediately
    ASSERT_TRUE(manager.unload_plugin("test034-sierra-tango-202-abc"));

    // Init remaining 3
    ASSERT_TRUE(manager.init_plugin("test034-quebec-romeo-201-xyz"));
    ASSERT_TRUE(manager.init_plugin("test034-uniform-victor-203-def"));
    ASSERT_TRUE(manager.init_plugin("test034-whiskey-xray-204-ghi"));

    // Unload plugin 3
    ASSERT_TRUE(manager.finit_plugin("test034-uniform-victor-203-def"));
    ASSERT_TRUE(manager.unload_plugin("test034-uniform-victor-203-def"));

    // Start remaining 2
    ASSERT_TRUE(manager.start_plugin("test034-quebec-romeo-201-xyz"));
    ASSERT_TRUE(manager.start_plugin("test034-whiskey-xray-204-ghi"));

    // Stop plugin 1
    ASSERT_TRUE(manager.stop_plugin("test034-whiskey-xray-204-ghi"));
    ASSERT_TRUE(manager.stop_plugin("test034-quebec-romeo-201-xyz"));

    // Finit plugin 4
    ASSERT_TRUE(manager.finit_plugin("test034-whiskey-xray-204-ghi"));
    ASSERT_TRUE(manager.finit_plugin("test034-quebec-romeo-201-xyz"));

    // Unload remaining
    ASSERT_TRUE(manager.unload_plugin("test034-quebec-romeo-201-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test034-whiskey-xray-204-ghi"));

    // Verify all unloaded
    ASSERT_FALSE(manager.is_plugin_loaded("test034-quebec-romeo-201-xyz"));
    ASSERT_FALSE(manager.is_plugin_loaded("test034-sierra-tango-202-abc"));
    ASSERT_FALSE(manager.is_plugin_loaded("test034-uniform-victor-203-def"));
    ASSERT_FALSE(manager.is_plugin_loaded("test034-whiskey-xray-204-ghi"));

    cleanup_test_file("test_config_rand_1.yaml");
    cleanup_test_file("test_config_rand_2.yaml");
    cleanup_test_file("test_config_rand_3.yaml");
    cleanup_test_file("test_config_rand_4.yaml");
}

// Test: Load 5 plugins, init all, start middle 3, stop first and last, finit all
TEST_F(PluginManagerTest, 039_MultiplePlugins_StartMiddleStopEnds_ShouldSucceed) {
    create_test_config("test_config_mid_1.yaml", "MiddlePlugin1", "test035-yankee-zulu-301-xyz");
    create_test_config("test_config_mid_2.yaml", "MiddlePlugin2", "test035-alpha-one-302-abc");
    create_test_config("test_config_mid_3.yaml", "MiddlePlugin3", "test035-bravo-two-303-def");
    create_test_config("test_config_mid_4.yaml", "MiddlePlugin4", "test035-charlie-three-304-ghi");
    create_test_config("test_config_mid_5.yaml", "MiddlePlugin5", "test035-delta-four-305-jkl");

    TestPluginManager manager;

    // Load all 5
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_mid_1.yaml", "test035-yankee-zulu-301-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_mid_2.yaml", "test035-alpha-one-302-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_mid_3.yaml", "test035-bravo-two-303-def"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_mid_4.yaml", "test035-charlie-three-304-ghi"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_mid_5.yaml", "test035-delta-four-305-jkl"));

    // Init all
    ASSERT_TRUE(manager.init_plugin("test035-yankee-zulu-301-xyz"));
    ASSERT_TRUE(manager.init_plugin("test035-alpha-one-302-abc"));
    ASSERT_TRUE(manager.init_plugin("test035-bravo-two-303-def"));
    ASSERT_TRUE(manager.init_plugin("test035-charlie-three-304-ghi"));
    ASSERT_TRUE(manager.init_plugin("test035-delta-four-305-jkl"));

    // Start middle 3 (2, 3, 4)
    ASSERT_TRUE(manager.start_plugin("test035-alpha-one-302-abc"));
    ASSERT_TRUE(manager.start_plugin("test035-bravo-two-303-def"));
    ASSERT_TRUE(manager.start_plugin("test035-charlie-three-304-ghi"));

    // Stop first and last (1 and 5) - should handle gracefully
    ASSERT_FALSE(manager.stop_plugin("test035-yankee-zulu-301-xyz"));
    ASSERT_FALSE(manager.stop_plugin("test035-delta-four-305-jkl"));

    // Stop middle 3
    ASSERT_TRUE(manager.stop_plugin("test035-alpha-one-302-abc"));
    ASSERT_TRUE(manager.stop_plugin("test035-bravo-two-303-def"));
    ASSERT_TRUE(manager.stop_plugin("test035-charlie-three-304-ghi"));

    // Finit all
    ASSERT_TRUE(manager.finit_plugin("test035-yankee-zulu-301-xyz"));
    ASSERT_TRUE(manager.finit_plugin("test035-alpha-one-302-abc"));
    ASSERT_TRUE(manager.finit_plugin("test035-bravo-two-303-def"));
    ASSERT_TRUE(manager.finit_plugin("test035-charlie-three-304-ghi"));
    ASSERT_TRUE(manager.finit_plugin("test035-delta-four-305-jkl"));

    // Unload all
    ASSERT_TRUE(manager.unload_plugin("test035-yankee-zulu-301-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test035-alpha-one-302-abc"));
    ASSERT_TRUE(manager.unload_plugin("test035-bravo-two-303-def"));
    ASSERT_TRUE(manager.unload_plugin("test035-charlie-three-304-ghi"));
    ASSERT_TRUE(manager.unload_plugin("test035-delta-four-305-jkl"));

    cleanup_test_file("test_config_mid_1.yaml");
    cleanup_test_file("test_config_mid_2.yaml");
    cleanup_test_file("test_config_mid_3.yaml");
    cleanup_test_file("test_config_mid_4.yaml");
    cleanup_test_file("test_config_mid_5.yaml");
}

// Test: Load 3 plugins, init only first, start only second (should fail or handle gracefully)
TEST_F(PluginManagerTest, 040_MultiplePlugins_StartWithoutInit_ShouldHandleGracefully) {
    create_test_config("test_config_uninit_1.yaml", "UninitPlugin1", "test036-echo-five-401-xyz");
    create_test_config("test_config_uninit_2.yaml", "UninitPlugin2", "test036-foxtrot-six-402-abc");
    create_test_config("test_config_uninit_3.yaml", "UninitPlugin3", "test036-golf-seven-403-def");

    TestPluginManager manager;

    // Load all
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_uninit_1.yaml", "test036-echo-five-401-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_uninit_2.yaml", "test036-foxtrot-six-402-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_uninit_3.yaml", "test036-golf-seven-403-def"));

    // Init only first
    ASSERT_TRUE(manager.init_plugin("test036-echo-five-401-xyz"));

    // Try to start second (not initialized) - behavior depends on implementation
    bool start_result = manager.start_plugin("test036-foxtrot-six-402-abc");

    // Init second and third
    ASSERT_TRUE(manager.init_plugin("test036-foxtrot-six-402-abc"));
    ASSERT_TRUE(manager.init_plugin("test036-golf-seven-403-def"));

    // Now start should work
    ASSERT_TRUE(manager.start_plugin("test036-foxtrot-six-402-abc"));

    // Cleanup properly
    ASSERT_TRUE(manager.stop_plugin("test036-foxtrot-six-402-abc"));
    ASSERT_TRUE(manager.finit_plugin("test036-echo-five-401-xyz"));
    ASSERT_TRUE(manager.finit_plugin("test036-foxtrot-six-402-abc"));
    ASSERT_TRUE(manager.finit_plugin("test036-golf-seven-403-def"));
    ASSERT_TRUE(manager.unload_plugin("test036-echo-five-401-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test036-foxtrot-six-402-abc"));
    ASSERT_TRUE(manager.unload_plugin("test036-golf-seven-403-def"));

    cleanup_test_file("test_config_uninit_1.yaml");
    cleanup_test_file("test_config_uninit_2.yaml");
    cleanup_test_file("test_config_uninit_3.yaml");
}

// Test: Load 4 plugins, unload middle 2, verify remaining work independently
TEST_F(PluginManagerTest, 041_MultiplePlugins_UnloadMiddleVerifyRemaining_ShouldSucceed) {
    create_test_config("test_config_rem_1.yaml", "RemainingPlugin1", "test037-hotel-eight-501-xyz");
    create_test_config("test_config_rem_2.yaml", "RemainingPlugin2", "test037-india-nine-502-abc");
    create_test_config("test_config_rem_3.yaml", "RemainingPlugin3", "test037-juliet-ten-503-def");
    create_test_config("test_config_rem_4.yaml", "RemainingPlugin4", "test037-kilo-eleven-504-ghi");

    TestPluginManager manager;

    // Load all 4
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rem_1.yaml", "test037-hotel-eight-501-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rem_2.yaml", "test037-india-nine-502-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rem_3.yaml", "test037-juliet-ten-503-def"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_rem_4.yaml", "test037-kilo-eleven-504-ghi"));

    // Init all
    ASSERT_TRUE(manager.init_plugin("test037-hotel-eight-501-xyz"));
    ASSERT_TRUE(manager.init_plugin("test037-india-nine-502-abc"));
    ASSERT_TRUE(manager.init_plugin("test037-juliet-ten-503-def"));
    ASSERT_TRUE(manager.init_plugin("test037-kilo-eleven-504-ghi"));

    // Finit middle 2 (2 and 3) before unloading
    ASSERT_TRUE(manager.finit_plugin("test037-india-nine-502-abc"));
    ASSERT_TRUE(manager.finit_plugin("test037-juliet-ten-503-def"));

    // Unload middle 2 (2 and 3)
    ASSERT_TRUE(manager.unload_plugin("test037-india-nine-502-abc"));
    ASSERT_TRUE(manager.unload_plugin("test037-juliet-ten-503-def"));

    // Verify remaining (1 and 4) still work
    ASSERT_TRUE(manager.is_plugin_loaded("test037-hotel-eight-501-xyz"));
    ASSERT_TRUE(manager.is_plugin_loaded("test037-kilo-eleven-504-ghi"));
    ASSERT_FALSE(manager.is_plugin_loaded("test037-india-nine-502-abc"));
    ASSERT_FALSE(manager.is_plugin_loaded("test037-juliet-ten-503-def"));

    // Start remaining
    ASSERT_TRUE(manager.start_plugin("test037-hotel-eight-501-xyz"));
    ASSERT_TRUE(manager.start_plugin("test037-kilo-eleven-504-ghi"));

    // Stop remaining
    ASSERT_TRUE(manager.stop_plugin("test037-hotel-eight-501-xyz"));
    ASSERT_TRUE(manager.stop_plugin("test037-kilo-eleven-504-ghi"));

    // Finit remaining
    ASSERT_TRUE(manager.finit_plugin("test037-hotel-eight-501-xyz"));
    ASSERT_TRUE(manager.finit_plugin("test037-kilo-eleven-504-ghi"));

    // Unload remaining
    ASSERT_TRUE(manager.unload_plugin("test037-hotel-eight-501-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test037-kilo-eleven-504-ghi"));

    cleanup_test_file("test_config_rem_1.yaml");
    cleanup_test_file("test_config_rem_2.yaml");
    cleanup_test_file("test_config_rem_3.yaml");
    cleanup_test_file("test_config_rem_4.yaml");
}

// Test: Load 5 plugins, perform operations in alternating pattern
TEST_F(PluginManagerTest, 042_MultiplePlugins_AlternatingOperations_ShouldSucceed) {
    create_test_config("test_config_alt_1.yaml", "AltPlugin1", "test038-lima-twelve-601-xyz");
    create_test_config("test_config_alt_2.yaml", "AltPlugin2", "test038-mike-thirteen-602-abc");
    create_test_config("test_config_alt_3.yaml", "AltPlugin3", "test038-november-fourteen-603-def");
    create_test_config("test_config_alt_4.yaml", "AltPlugin4", "test038-oscar-fifteen-604-ghi");
    create_test_config("test_config_alt_5.yaml", "AltPlugin5", "test038-papa-sixteen-605-jkl");

    TestPluginManager manager;

    // Load all
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_alt_1.yaml", "test038-lima-twelve-601-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_alt_2.yaml", "test038-mike-thirteen-602-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_alt_3.yaml", "test038-november-fourteen-603-def"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_alt_4.yaml", "test038-oscar-fifteen-604-ghi"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_alt_5.yaml", "test038-papa-sixteen-605-jkl"));


    // Init odd plugins (1, 3, 5)
    ASSERT_TRUE(manager.init_plugin("test038-lima-twelve-601-xyz"));
    ASSERT_TRUE(manager.init_plugin("test038-november-fourteen-603-def"));
    ASSERT_TRUE(manager.init_plugin("test038-papa-sixteen-605-jkl"));

    // Start even plugins (2, 4) - may fail if not initialized
    manager.start_plugin("test038-mike-thirteen-602-abc");
    manager.start_plugin("test038-oscar-fifteen-604-ghi");

    // Init even plugins
    ASSERT_TRUE(manager.init_plugin("test038-mike-thirteen-602-abc"));
    ASSERT_TRUE(manager.init_plugin("test038-oscar-fifteen-604-ghi"));

    // Start all
    ASSERT_TRUE(manager.start_plugin("test038-lima-twelve-601-xyz"));
    ASSERT_TRUE(manager.start_plugin("test038-mike-thirteen-602-abc"));
    ASSERT_TRUE(manager.start_plugin("test038-november-fourteen-603-def"));
    ASSERT_TRUE(manager.start_plugin("test038-oscar-fifteen-604-ghi"));
    ASSERT_TRUE(manager.start_plugin("test038-papa-sixteen-605-jkl"));

    // Stop even plugins
    ASSERT_TRUE(manager.stop_plugin("test038-mike-thirteen-602-abc"));
    ASSERT_TRUE(manager.stop_plugin("test038-oscar-fifteen-604-ghi"));

    // Finit odd plugins
    ASSERT_FALSE(manager.finit_plugin("test038-lima-twelve-601-xyz"));
    ASSERT_FALSE(manager.finit_plugin("test038-november-fourteen-603-def"));
    ASSERT_FALSE(manager.finit_plugin("test038-papa-sixteen-605-jkl"));

    // Stop odd plugins
    ASSERT_TRUE(manager.stop_plugin("test038-lima-twelve-601-xyz"));
    ASSERT_TRUE(manager.stop_plugin("test038-november-fourteen-603-def"));
    ASSERT_TRUE(manager.stop_plugin("test038-papa-sixteen-605-jkl"));

    // Finit even plugins
    ASSERT_TRUE(manager.finit_plugin("test038-mike-thirteen-602-abc"));
    ASSERT_TRUE(manager.finit_plugin("test038-oscar-fifteen-604-ghi"));

    // Stop all plugins first
    ASSERT_FALSE(manager.stop_plugin("test038-lima-twelve-601-xyz"));
    ASSERT_FALSE(manager.stop_plugin("test038-mike-thirteen-602-abc"));
    ASSERT_FALSE(manager.stop_plugin("test038-november-fourteen-603-def"));
    ASSERT_FALSE(manager.stop_plugin("test038-oscar-fifteen-604-ghi"));
    ASSERT_FALSE(manager.stop_plugin("test038-papa-sixteen-605-jkl"));

    // Finit all plugins
    ASSERT_TRUE(manager.finit_plugin("test038-lima-twelve-601-xyz"));
    ASSERT_FALSE(manager.finit_plugin("test038-mike-thirteen-602-abc"));
    ASSERT_TRUE(manager.finit_plugin("test038-november-fourteen-603-def"));
    ASSERT_FALSE(manager.finit_plugin("test038-oscar-fifteen-604-ghi"));
    ASSERT_TRUE(manager.finit_plugin("test038-papa-sixteen-605-jkl"));

    // Unload all
    ASSERT_TRUE(manager.unload_plugin("test038-lima-twelve-601-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test038-mike-thirteen-602-abc"));
    ASSERT_TRUE(manager.unload_plugin("test038-november-fourteen-603-def"));
    ASSERT_TRUE(manager.unload_plugin("test038-oscar-fifteen-604-ghi"));
    ASSERT_TRUE(manager.unload_plugin("test038-papa-sixteen-605-jkl"));

    cleanup_test_file("test_config_alt_1.yaml");
    cleanup_test_file("test_config_alt_2.yaml");
    cleanup_test_file("test_config_alt_3.yaml");
    cleanup_test_file("test_config_alt_4.yaml");
    cleanup_test_file("test_config_alt_5.yaml");
}

// Test: Load 3 plugins, start all, unload one while others running
TEST_F(PluginManagerTest, 043_MultiplePlugins_UnloadWhileOthersRunning_ShouldSucceed) {
    create_test_config("test_config_run_1.yaml", "RunningPlugin1", "test039-quebec-seventeen-701-xyz");
    create_test_config("test_config_run_2.yaml", "RunningPlugin2", "test039-romeo-eighteen-702-abc");
    create_test_config("test_config_run_3.yaml", "RunningPlugin3", "test039-sierra-nineteen-703-def");

    TestPluginManager manager;

    // Load all
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_run_1.yaml", "test039-quebec-seventeen-701-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_run_2.yaml", "test039-romeo-eighteen-702-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_run_3.yaml", "test039-sierra-nineteen-703-def"));

    // Init all
    ASSERT_TRUE(manager.init_plugin("test039-quebec-seventeen-701-xyz"));
    ASSERT_TRUE(manager.init_plugin("test039-romeo-eighteen-702-abc"));
    ASSERT_TRUE(manager.init_plugin("test039-sierra-nineteen-703-def"));

    // Start all
    ASSERT_TRUE(manager.start_plugin("test039-quebec-seventeen-701-xyz"));
    ASSERT_TRUE(manager.start_plugin("test039-romeo-eighteen-702-abc"));
    ASSERT_TRUE(manager.start_plugin("test039-sierra-nineteen-703-def"));

    // Unload middle plugin while others are running - should fail
    ASSERT_FALSE(manager.unload_plugin("test039-romeo-eighteen-702-abc"));
    // Stop and finit the plugin first
    ASSERT_TRUE(manager.stop_plugin("test039-romeo-eighteen-702-abc"));
    ASSERT_TRUE(manager.finit_plugin("test039-romeo-eighteen-702-abc"));
    // Now unload should succeed
    ASSERT_TRUE(manager.unload_plugin("test039-romeo-eighteen-702-abc"));

    // Verify others still loaded and running
    ASSERT_TRUE(manager.is_plugin_loaded("test039-quebec-seventeen-701-xyz"));
    ASSERT_FALSE(manager.is_plugin_loaded("test039-romeo-eighteen-702-abc"));
    ASSERT_TRUE(manager.is_plugin_loaded("test039-sierra-nineteen-703-def"));

    // Stop remaining
    ASSERT_TRUE(manager.stop_plugin("test039-quebec-seventeen-701-xyz"));
    ASSERT_TRUE(manager.stop_plugin("test039-sierra-nineteen-703-def"));

    // Finit remaining
    ASSERT_TRUE(manager.finit_plugin("test039-quebec-seventeen-701-xyz"));
    ASSERT_TRUE(manager.finit_plugin("test039-sierra-nineteen-703-def"));

    // Unload remaining
    ASSERT_TRUE(manager.unload_plugin("test039-quebec-seventeen-701-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test039-sierra-nineteen-703-def"));

    cleanup_test_file("test_config_run_1.yaml");
    cleanup_test_file("test_config_run_2.yaml");
    cleanup_test_file("test_config_run_3.yaml");
}

// Test: Load 4 plugins, init in reverse order, start in original order
TEST_F(PluginManagerTest, 044_MultiplePlugins_InitReverseStartOriginal_ShouldSucceed) {
    create_test_config("test_config_order_1.yaml", "OrderPlugin1", "test040-tango-twenty-801-xyz");
    create_test_config("test_config_order_2.yaml", "OrderPlugin2", "test040-uniform-twentyone-802-abc");
    create_test_config("test_config_order_3.yaml", "OrderPlugin3", "test040-victor-twentytwo-803-def");
    create_test_config("test_config_order_4.yaml", "OrderPlugin4", "test040-whiskey-twentythree-804-ghi");

    TestPluginManager manager;

    // Load all
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_order_1.yaml", "test040-tango-twenty-801-xyz"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_order_2.yaml", "test040-uniform-twentyone-802-abc"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_order_3.yaml", "test040-victor-twentytwo-803-def"));
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_order_4.yaml", "test040-whiskey-twentythree-804-ghi"));

    // Init in reverse order (4, 3, 2, 1)
    ASSERT_TRUE(manager.init_plugin("test040-whiskey-twentythree-804-ghi"));
    ASSERT_TRUE(manager.init_plugin("test040-victor-twentytwo-803-def"));
    ASSERT_TRUE(manager.init_plugin("test040-uniform-twentyone-802-abc"));
    ASSERT_TRUE(manager.init_plugin("test040-tango-twenty-801-xyz"));

    // Start in original order (1, 2, 3, 4)
    ASSERT_TRUE(manager.start_plugin("test040-tango-twenty-801-xyz"));
    ASSERT_TRUE(manager.start_plugin("test040-uniform-twentyone-802-abc"));
    ASSERT_TRUE(manager.start_plugin("test040-victor-twentytwo-803-def"));
    ASSERT_TRUE(manager.start_plugin("test040-whiskey-twentythree-804-ghi"));

    // Stop all
    ASSERT_TRUE(manager.stop_plugin("test040-tango-twenty-801-xyz"));
    ASSERT_TRUE(manager.stop_plugin("test040-uniform-twentyone-802-abc"));
    ASSERT_TRUE(manager.stop_plugin("test040-victor-twentytwo-803-def"));
    ASSERT_TRUE(manager.stop_plugin("test040-whiskey-twentythree-804-ghi"));

    // Finit all
    ASSERT_TRUE(manager.finit_plugin("test040-tango-twenty-801-xyz"));
    ASSERT_TRUE(manager.finit_plugin("test040-uniform-twentyone-802-abc"));
    ASSERT_TRUE(manager.finit_plugin("test040-victor-twentytwo-803-def"));
    ASSERT_TRUE(manager.finit_plugin("test040-whiskey-twentythree-804-ghi"));

    // Unload all
    ASSERT_TRUE(manager.unload_plugin("test040-tango-twenty-801-xyz"));
    ASSERT_TRUE(manager.unload_plugin("test040-uniform-twentyone-802-abc"));
    ASSERT_TRUE(manager.unload_plugin("test040-victor-twentytwo-803-def"));
    ASSERT_TRUE(manager.unload_plugin("test040-whiskey-twentythree-804-ghi"));

    cleanup_test_file("test_config_order_1.yaml");
    cleanup_test_file("test_config_order_2.yaml");
    cleanup_test_file("test_config_order_3.yaml");
    cleanup_test_file("test_config_order_4.yaml");
}

// ============================================================================
// TESTS FOR REDUNDANT OPERATIONS (IDEMPOTENCY)
// ============================================================================

// Test: Redundant load of same plugin with same UUID should fail (not idempotent)
TEST_F(PluginManagerTest, 045_RedundantLoad_SameUUID_ShouldFail) {
    create_test_config("test_config_red_load.yaml", "RedundantLoadPlugin", "test041-red-load-uuid-abc");

    TestPluginManager manager;
    bool result1 = manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_load.yaml", "test041-red-load-uuid-abc");
    bool result2 = manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_load.yaml", "test041-red-load-uuid-abc");

    cleanup_test_file("test_config_red_load.yaml");

    ASSERT_TRUE(result1) << "First load should succeed";
    ASSERT_FALSE(result2) << "Redundant load should fail (plugin already loaded)";
    ASSERT_TRUE(manager.is_plugin_loaded("test041-red-load-uuid-abc")) << "Plugin should be loaded";
}

// Test: Redundant init of same plugin should fail (not idempotent)
TEST_F(PluginManagerTest, 046_RedundantInit_ShouldFail) {
    create_test_config("test_config_red_init.yaml", "RedundantInitPlugin", "test042-red-init-uuid-def");

    TestPluginManager manager;

    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_init.yaml", "test042-red-init-uuid-def");
    bool result1 = manager.init_plugin("test042-red-init-uuid-def");
    bool result2 = manager.init_plugin("test042-red-init-uuid-def");

    // Clean up properly
    bool finit_result = manager.finit_plugin("test042-red-init-uuid-def");
    bool unload_result = manager.unload_plugin("test042-red-init-uuid-def");

    cleanup_test_file("test_config_red_init.yaml");

    ASSERT_TRUE(result1) << "First init should succeed";
    ASSERT_FALSE(result2) << "Redundant init should fail (plugin already initialized)";
    ASSERT_TRUE(finit_result) << "finit_plugin should succeed";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: Redundant start of same plugin should fail (not idempotent)
TEST_F(PluginManagerTest, 047_RedundantStart_ShouldFail) {
    create_test_config("test_config_red_start.yaml", "RedundantStartPlugin", "test043-red-start-uuid-ghi");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_start.yaml", "test043-red-start-uuid-ghi");
    manager.init_plugin("test043-red-start-uuid-ghi");
    bool result1 = manager.start_plugin("test043-red-start-uuid-ghi");
    bool result2 = manager.start_plugin("test043-red-start-uuid-ghi");

    // Clean up properly
    bool stop_result = manager.stop_plugin("test043-red-start-uuid-ghi");
    bool finit_result = manager.finit_plugin("test043-red-start-uuid-ghi");
    bool unload_result = manager.unload_plugin("test043-red-start-uuid-ghi");

    cleanup_test_file("test_config_red_start.yaml");

    ASSERT_TRUE(result1) << "First start should succeed";
    ASSERT_FALSE(result2) << "Redundant start should fail (plugin already started)";
    ASSERT_TRUE(stop_result) << "stop_plugin should succeed";
    ASSERT_TRUE(finit_result) << "finit_plugin should succeed";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: Redundant stop of same plugin should fail (not idempotent)
TEST_F(PluginManagerTest, 048_RedundantStop_ShouldFail) {
    create_test_config("test_config_red_stop.yaml", "RedundantStopPlugin", "test044-red-stop-uuid-jkl");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_stop.yaml", "test044-red-stop-uuid-jkl");
    manager.init_plugin("test044-red-stop-uuid-jkl");
    manager.start_plugin("test044-red-stop-uuid-jkl");
    bool result1 = manager.stop_plugin("test044-red-stop-uuid-jkl");
    bool result2 = manager.stop_plugin("test044-red-stop-uuid-jkl");

    // Clean up properly
    bool finit_result = manager.finit_plugin("test044-red-stop-uuid-jkl");
    bool unload_result = manager.unload_plugin("test044-red-stop-uuid-jkl");

    cleanup_test_file("test_config_red_stop.yaml");

    ASSERT_TRUE(result1) << "First stop should succeed";
    ASSERT_FALSE(result2) << "Redundant stop should fail (plugin already stopped)";
    ASSERT_TRUE(finit_result) << "finit_plugin should succeed";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: Redundant finit of same plugin should fail (not idempotent)
TEST_F(PluginManagerTest, 049_RedundantFinit_ShouldFail) {
    create_test_config("test_config_red_finit.yaml", "RedundantFinitPlugin", "test045-red-finit-uuid-mno");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_finit.yaml", "test045-red-finit-uuid-mno");
    manager.init_plugin("test045-red-finit-uuid-mno");
    manager.start_plugin("test045-red-finit-uuid-mno");
    manager.stop_plugin("test045-red-finit-uuid-mno");
    bool result1 = manager.finit_plugin("test045-red-finit-uuid-mno");
    bool result2 = manager.finit_plugin("test045-red-finit-uuid-mno");

    // Clean up properly
    bool unload_result = manager.unload_plugin("test045-red-finit-uuid-mno");

    cleanup_test_file("test_config_red_finit.yaml");

    ASSERT_TRUE(result1) << "First finit should succeed";
    ASSERT_FALSE(result2) << "Redundant finit should fail (plugin already finited)";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: Redundant unload of same plugin should fail after first unload (not idempotent)
TEST_F(PluginManagerTest, 050_RedundantUnload_ShouldFailAfterFirst) {
    create_test_config("test_config_red_unload.yaml", "RedundantUnloadPlugin", "test046-red-unload-uuid-pqr");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_unload.yaml", "test046-red-unload-uuid-pqr");
    manager.init_plugin("test046-red-unload-uuid-pqr");
    manager.start_plugin("test046-red-unload-uuid-pqr");
    manager.stop_plugin("test046-red-unload-uuid-pqr");
    manager.finit_plugin("test046-red-unload-uuid-pqr");
    bool result1 = manager.unload_plugin("test046-red-unload-uuid-pqr");
    bool result2 = manager.unload_plugin("test046-red-unload-uuid-pqr");

    cleanup_test_file("test_config_red_unload.yaml");

    ASSERT_TRUE(result1) << "First unload should succeed";
    ASSERT_FALSE(result2) << "Redundant unload should fail (plugin already unloaded)";
    ASSERT_FALSE(manager.is_plugin_loaded("test046-red-unload-uuid-pqr")) << "Plugin should be unloaded";
}

// Test: Full redundant lifecycle - load, init, start, stop, finit, unload, then repeat
// Second round should succeed as it's a fresh start after complete lifecycle
TEST_F(PluginManagerTest, 051_RedundantLifecycle_ShouldSucceed) {
    create_test_config("test_config_red_lifecycle.yaml", "RedundantLifecyclePlugin", "test047-red-lifecycle-uuid-stu");

    TestPluginManager manager;

    // First lifecycle
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_lifecycle.yaml", "test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.init_plugin("test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.start_plugin("test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.stop_plugin("test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.finit_plugin("test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.unload_plugin("test047-red-lifecycle-uuid-stu"));

    // Second lifecycle (fresh start after complete first lifecycle)
    ASSERT_TRUE(manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_lifecycle.yaml", "test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.init_plugin("test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.start_plugin("test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.stop_plugin("test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.finit_plugin("test047-red-lifecycle-uuid-stu"));
    ASSERT_TRUE(manager.unload_plugin("test047-red-lifecycle-uuid-stu"));

    cleanup_test_file("test_config_red_lifecycle.yaml");
}

// Test: Redundant operations on non-existent plugin should fail gracefully
TEST_F(PluginManagerTest, 052_RedundantOperations_NonExistentPlugin_ShouldFailGracefully) {
    TestPluginManager manager;

    // All operations on non-existent plugin should fail
    ASSERT_FALSE(manager.init_plugin("test048-non-existent-uuid-123"));
    ASSERT_FALSE(manager.start_plugin("test048-non-existent-uuid-123"));
    ASSERT_FALSE(manager.stop_plugin("test048-non-existent-uuid-123"));
    ASSERT_FALSE(manager.finit_plugin("test048-non-existent-uuid-123"));
    ASSERT_FALSE(manager.unload_plugin("test048-non-existent-uuid-123"));
}

// Test: Redundant get_plugin_capabilities should return consistent results
TEST_F(PluginManagerTest, 053_RedundantGetCapabilities_ShouldBeConsistent) {
    create_test_config("test_config_red_caps.yaml", "RedundantCapsPlugin", "test049-red-caps-uuid-vwx");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_caps.yaml", "test049-red-caps-uuid-vwx");

    std::vector<std::string> caps1 = manager.get_plugin_capabilities("test049-red-caps-uuid-vwx");
    std::vector<std::string> caps2 = manager.get_plugin_capabilities("test049-red-caps-uuid-vwx");
    std::vector<std::string> caps3 = manager.get_plugin_capabilities("test049-red-caps-uuid-vwx");

    // Clean up properly
    bool unload_result = manager.unload_plugin("test049-red-caps-uuid-vwx");

    cleanup_test_file("test_config_red_caps.yaml");

    ASSERT_EQ(caps1.size(), caps2.size()) << "Capabilities should be consistent between calls";
    ASSERT_EQ(caps2.size(), caps3.size()) << "Capabilities should be consistent between calls";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: Redundant get_loaded_plugins should return consistent results
TEST_F(PluginManagerTest, 054_RedundantGetLoadedPlugins_ShouldBeConsistent) {
    create_test_config("test_config_red_loaded.yaml", "RedundantLoadedPlugin", "test050-red-loaded-uuid-yza");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_loaded.yaml", "test050-red-loaded-uuid-yza");

    std::vector<std::string> loaded1 = manager.get_loaded_plugins();
    std::vector<std::string> loaded2 = manager.get_loaded_plugins();
    std::vector<std::string> loaded3 = manager.get_loaded_plugins();

    // Clean up properly
    bool unload_result = manager.unload_plugin("test050-red-loaded-uuid-yza");

    cleanup_test_file("test_config_red_loaded.yaml");

    ASSERT_EQ(loaded1.size(), loaded2.size()) << "Loaded plugins list should be consistent between calls";
    ASSERT_EQ(loaded2.size(), loaded3.size()) << "Loaded plugins list should be consistent between calls";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: Redundant print_loaded_plugins should not crash
TEST_F(PluginManagerTest, 055_RedundantPrintLoadedPlugins_ShouldNotCrash) {
    create_test_config("test_config_red_print.yaml", "RedundantPrintPlugin", "test051-red-print-uuid-bcd");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_red_print.yaml", "test051-red-print-uuid-bcd");

    // Call print multiple times
    manager.print_loaded_plugins();
    manager.print_loaded_plugins();
    manager.print_loaded_plugins();

    cleanup_test_file("test_config_red_print.yaml");
    manager.unload_plugin("test051-red-print-uuid-bcd");
}

// ============================================================================
// TESTS FOR get_plugin_capabilities() FUNCTION
// ============================================================================

// Test: get_plugin_capabilities should return capabilities for a loaded plugin
TEST_F(PluginManagerTest, 056_GetPluginCapabilities_ValidPlugin_ShouldReturnCapabilities) {
    create_test_config("test_config_caps.yaml", "CapabilitiesPlugin", "test052-caps-uuid-efg");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_caps.yaml", "test052-caps-uuid-efg");

    std::vector<std::string> capabilities = manager.get_plugin_capabilities("test052-caps-uuid-efg");

    // Clean up properly
    bool unload_result = manager.unload_plugin("test052-caps-uuid-efg");

    cleanup_test_file("test_config_caps.yaml");

    // Should return some capabilities (at least empty vector, not crash)
    ASSERT_FALSE(capabilities.empty()) << "Plugin should have capabilities";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: get_plugin_capabilities should return empty vector for non-existent plugin
TEST_F(PluginManagerTest, 057_GetPluginCapabilities_NonExistentPlugin_ShouldReturnEmpty) {
    TestPluginManager manager;

    std::vector<std::string> capabilities = manager.get_plugin_capabilities("test053-non-existent-uuid-hij");

    ASSERT_TRUE(capabilities.empty()) << "Non-existent plugin should return empty capabilities";
}

// Test: get_plugin_capabilities should return empty vector for unloaded plugin
TEST_F(PluginManagerTest, 058_GetPluginCapabilities_UnloadedPlugin_ShouldReturnEmpty) {
    create_test_config("test_config_caps_unloaded.yaml", "CapsUnloadedPlugin", "test054-caps-unloaded-uuid-klm");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_caps_unloaded.yaml", "test054-caps-unloaded-uuid-klm");
    manager.unload_plugin("test054-caps-unloaded-uuid-klm");

    std::vector<std::string> capabilities = manager.get_plugin_capabilities("test054-caps-unloaded-uuid-klm");

    cleanup_test_file("test_config_caps_unloaded.yaml");

    ASSERT_TRUE(capabilities.empty()) << "Unloaded plugin should return empty capabilities";
}

// Test: get_plugin_capabilities should return consistent results for same plugin
TEST_F(PluginManagerTest, 059_GetPluginCapabilities_Consistency_ShouldMatch) {
    create_test_config("test_config_caps_consistent.yaml", "CapsConsistentPlugin", "test055-caps-consistent-uuid-nop");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_caps_consistent.yaml", "test055-caps-consistent-uuid-nop");

    std::vector<std::string> caps1 = manager.get_plugin_capabilities("test055-caps-consistent-uuid-nop");
    std::vector<std::string> caps2 = manager.get_plugin_capabilities("test055-caps-consistent-uuid-nop");

    cleanup_test_file("test_config_caps_consistent.yaml");

    ASSERT_EQ(caps1.size(), caps2.size()) << "Capabilities should be consistent";
    for (size_t i = 0; i < caps1.size(); ++i) {
        ASSERT_EQ(caps1[i], caps2[i]) << "Capability " << i << " should match";
    }
}

// ============================================================================
// TESTS FOR get_loaded_plugins() FUNCTION
// ============================================================================

// Test: get_loaded_plugins should return list of loaded plugins
TEST_F(PluginManagerTest, 060_GetLoadedPlugins_ValidPlugins_ShouldReturnList) {
    create_test_config("test_config_loaded_1.yaml", "LoadedPlugin1", "test056-loaded-1-uuid-qrs");
    create_test_config("test_config_loaded_2.yaml", "LoadedPlugin2", "test056-loaded-2-uuid-tuv");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_loaded_1.yaml", "test056-loaded-1-uuid-qrs");
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_loaded_2.yaml", "test056-loaded-2-uuid-tuv");

    std::vector<std::string> loaded = manager.get_loaded_plugins();

    cleanup_test_file("test_config_loaded_1.yaml");
    cleanup_test_file("test_config_loaded_2.yaml");

    ASSERT_EQ(loaded.size(), 2) << "Should have 2 loaded plugins";
    ASSERT_TRUE(std::find(loaded.begin(), loaded.end(), "test056-loaded-1-uuid-qrs") != loaded.end());
    ASSERT_TRUE(std::find(loaded.begin(), loaded.end(), "test056-loaded-2-uuid-tuv") != loaded.end());
}

// Test: get_loaded_plugins should return empty list when no plugins loaded
TEST_F(PluginManagerTest, 061_GetLoadedPlugins_NoPlugins_ShouldReturnEmpty) {
    TestPluginManager manager;

    std::vector<std::string> loaded = manager.get_loaded_plugins();

    ASSERT_TRUE(loaded.empty()) << "Should return empty list when no plugins loaded";
}

// Test: get_loaded_plugins should reflect plugin unloading
TEST_F(PluginManagerTest, 062_GetLoadedPlugins_AfterUnload_ShouldReflectChanges) {
    create_test_config("test_config_loaded_unload.yaml", "LoadedUnloadPlugin", "test056-unload-uuid-wxy");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_loaded_unload.yaml", "test056-unload-uuid-wxy");

    std::vector<std::string> loaded1 = manager.get_loaded_plugins();
    manager.unload_plugin("test056-unload-uuid-wxy");
    std::vector<std::string> loaded2 = manager.get_loaded_plugins();

    cleanup_test_file("test_config_loaded_unload.yaml");

    ASSERT_EQ(loaded1.size(), 1) << "Should have 1 plugin before unload";
    ASSERT_EQ(loaded2.size(), 0) << "Should have 0 plugins after unload";
}

// Test: get_loaded_plugins should return unique plugin UUIDs
TEST_F(PluginManagerTest, 063_GetLoadedPlugins_Uniqueness_ShouldNotDuplicate) {
    create_test_config("test_config_unique_1.yaml", "UniquePlugin1", "test059-unique-1-uuid-zab");
    create_test_config("test_config_unique_2.yaml", "UniquePlugin2", "test059-unique-2-uuid-cde");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_unique_1.yaml", "test059-unique-1-uuid-zab");
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_unique_2.yaml", "test059-unique-2-uuid-cde");
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_unique_1.yaml", "test059-unique-1-uuid-zab"); // Load same plugin again

    std::vector<std::string> loaded = manager.get_loaded_plugins();

    cleanup_test_file("test_config_unique_1.yaml");
    cleanup_test_file("test_config_unique_2.yaml");

    // Check for duplicates
    std::set<std::string> unique_uuids(loaded.begin(), loaded.end());
    ASSERT_EQ(loaded.size(), unique_uuids.size()) << "Loaded plugins list should not contain duplicates";
}

// ============================================================================
// TESTS FOR print_loaded_plugins() FUNCTION
// ============================================================================

// Test: print_loaded_plugins should not crash with no plugins loaded
TEST_F(PluginManagerTest, 064_PrintLoadedPlugins_NoPlugins_ShouldNotCrash) {
    TestPluginManager manager;

    // Should not crash
    manager.print_loaded_plugins();
}

// Test: print_loaded_plugins should not crash with plugins loaded
TEST_F(PluginManagerTest, 065_PrintLoadedPlugins_WithPlugins_ShouldNotCrash) {
    create_test_config("test_config_print_multi.yaml", "PrintMultiPlugin", "test061-print-multi-uuid-fgh");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_print_multi.yaml", "test061-print-multi-uuid-fgh");

    // Should not crash
    manager.print_loaded_plugins();

    cleanup_test_file("test_config_print_multi.yaml");
    manager.unload_plugin("test061-print-multi-uuid-fgh");
}

// Test: print_loaded_plugins should reflect plugin lifecycle changes
TEST_F(PluginManagerTest, 066_PrintLoadedPlugins_LifecycleChanges_ShouldReflect) {
    create_test_config("test_config_print_lifecycle.yaml", "PrintLifecyclePlugin", "test061-lifecycle-uuid-ijk");

    TestPluginManager manager;

    // Should not crash with no plugins
    manager.print_loaded_plugins();


    manager.load_plugin("../plugins/test" PLUGIN_EXT, "test_config_print_lifecycle.yaml", "test061-lifecycle-uuid-ijk");
    // Should not crash with one plugin
    manager.print_loaded_plugins();

    manager.init_plugin("test061-lifecycle-uuid-ijk");
    // Should not crash after init
    manager.print_loaded_plugins();

    manager.start_plugin("test061-lifecycle-uuid-ijk");
    // Should not crash after start
    manager.print_loaded_plugins();

    manager.stop_plugin("test061-lifecycle-uuid-ijk");
    // Should not crash after stop
    manager.print_loaded_plugins();

    manager.finit_plugin("test061-lifecycle-uuid-ijk");
    // Should not crash after finit
    manager.print_loaded_plugins();

    manager.unload_plugin("test061-lifecycle-uuid-ijk");
    // Should not crash after unload
    manager.print_loaded_plugins();

    cleanup_test_file("test_config_print_lifecycle.yaml");
}

TEST_F(
    PluginManagerTest,
    074_PeriodicStatusReporting_ReflectsPluginLoadAndUnload)
{
    const std::string uuidA = "test074-status-report-plugin-a";
    const std::string uuidB = "test074-status-report-plugin-b";
    create_test_config("test_config_status_a.yaml", "AlphaPlugin", uuidA);
    create_test_config("test_config_status_b.yaml", "BetaPlugin", uuidB);
    {
        std::ofstream telemetryCollectorConfigFile(
            "test_collector_status_report.yaml");
        telemetryCollectorConfigFile
            << "plugin_status_interval_ms: 10\n"
            << "seanerbus:\n"
            << "  url: seanerbroker://127.0.0.1:1\n"
            << "plugins:\n"
            << "  - path: unused\n"
            << "    config: test_config_status_a.yaml\n"
            << "    uuid: " << uuidA << "\n"
            << "    name: AlphaPlugin\n"
            << "  - path: unused\n"
            << "    config: test_config_status_b.yaml\n"
            << "    uuid: " << uuidB << "\n"
            << "    name: BetaPlugin\n";
    }

    PluginManager manager;
    ASSERT_TRUE(manager.load_collector_config(
        "test_collector_status_report.yaml"));

    testing::internal::CaptureStdout();
    ASSERT_TRUE(manager.start_loaded_plugin_reporting());
    std::this_thread::sleep_for(std::chrono::milliseconds(35));

    ASSERT_TRUE(manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "test_config_status_a.yaml",
        uuidA.c_str(),
        "test://localhost"));
    std::this_thread::sleep_for(std::chrono::milliseconds(35));

    ASSERT_TRUE(manager.load_plugin(
        "../plugins/test" PLUGIN_EXT,
        "test_config_status_b.yaml",
        uuidB.c_str(),
        "test://localhost"));
    std::this_thread::sleep_for(std::chrono::milliseconds(35));

    ASSERT_TRUE(manager.init_plugin(uuidA.c_str()));
    ASSERT_TRUE(manager.finit_plugin(uuidA.c_str()));
    ASSERT_TRUE(manager.unload_plugin(uuidA.c_str()));
    std::this_thread::sleep_for(std::chrono::milliseconds(35));

    ASSERT_TRUE(manager.init_plugin(uuidB.c_str()));
    ASSERT_TRUE(manager.finit_plugin(uuidB.c_str()));
    ASSERT_TRUE(manager.unload_plugin(uuidB.c_str()));
    std::this_thread::sleep_for(std::chrono::milliseconds(35));

    manager.stop_loaded_plugin_reporting();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto output = testing::internal::GetCapturedStdout();

    struct ReportSnapshot
    {
        std::size_t count;
        std::set<std::string> plugins;
    };
    std::vector<ReportSnapshot> reports;
    std::istringstream lines(output);
    std::string line;
    const auto alphaEntry = "AlphaPlugin (" + uuidA + ")";
    const auto betaEntry = "BetaPlugin (" + uuidB + ")";
    while (std::getline(lines, line))
    {
        if (line.find("Currently loaded plugins: none") != std::string::npos)
            reports.push_back({0, {}});
        else if (line.find("Currently loaded plugins (1):") != std::string::npos)
            reports.push_back({1, {}});
        else if (line.find("Currently loaded plugins (2):") != std::string::npos)
            reports.push_back({2, {}});

        if (reports.empty())
            continue;
        if (line.find(alphaEntry) != std::string::npos)
            reports.back().plugins.insert(alphaEntry);
        if (line.find(betaEntry) != std::string::npos)
            reports.back().plugins.insert(betaEntry);
    }

    const auto findSnapshot =
        [&reports](std::size_t start,
                   std::size_t count,
                   const std::set<std::string> &plugins)
    {
        for (auto index = start; index < reports.size(); ++index)
        {
            if (reports[index].count == count &&
                reports[index].plugins == plugins)
            {
                return index;
            }
        }
        return reports.size();
    };

    const std::set<std::string> noPlugins;
    const std::set<std::string> alphaOnly{alphaEntry};
    const std::set<std::string> bothPlugins{alphaEntry, betaEntry};
    const std::set<std::string> betaOnly{betaEntry};

    auto next = findSnapshot(0, 0, noPlugins);
    ASSERT_LT(next, reports.size()) << "No empty-plugin report was logged";
    next = findSnapshot(next + 1, 1, alphaOnly);
    ASSERT_LT(next, reports.size()) << "The loaded alpha plugin was not reported";
    next = findSnapshot(next + 1, 2, bothPlugins);
    ASSERT_LT(next, reports.size()) << "Both loaded plugins were not reported";
    next = findSnapshot(next + 1, 1, betaOnly);
    ASSERT_LT(next, reports.size()) << "The remaining beta plugin was not reported";
    next = findSnapshot(next + 1, 0, noPlugins);
    ASSERT_LT(next, reports.size()) << "The unloaded state was not reported";

    cleanup_test_file("test_config_status_a.yaml");
    cleanup_test_file("test_config_status_b.yaml");
    cleanup_test_file("test_collector_status_report.yaml");
}

// ============================================================================
// TESTS FOR run() FUNCTION REQUIREMENT
// ============================================================================

// Test: Plugin without run function should be rejected during load
TEST_F(PluginManagerTest, 067_LoadPlugin_MissingRun_ShouldBeRejected) {
    create_test_config("test_config_no_run.yaml", "NoRunPlugin", "test063-no-run-uuid-abc");

    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test_plugin_no_run" PLUGIN_EXT,
        "test_config_no_run.yaml",
        "test063-no-run-uuid-abc");

    cleanup_test_file("test_config_no_run.yaml");

    ASSERT_FALSE(result) << "Plugin should be rejected for missing run function";
    ASSERT_FALSE(manager.is_plugin_loaded("test063-no-run-uuid-abc")) << "Plugin should not be loaded";
}

// Test: Plugin with run function should load successfully
TEST_F(PluginManagerTest, 068_LoadPlugin_WithRun_ShouldSucceed) {
    create_test_config("test_config_with_run.yaml", "WithRunPlugin", "test064-with-run-uuid-def");

    TestPluginManager manager;
    bool result = manager.load_plugin(
        "../plugins/test_plugin_with_run" PLUGIN_EXT,
        "test_config_with_run.yaml",
        "test064-with-run-uuid-def");

    cleanup_test_file("test_config_with_run.yaml");

    ASSERT_TRUE(result) << "Plugin with run function should load successfully";
    ASSERT_TRUE(manager.is_plugin_loaded("test064-with-run-uuid-def")) << "Plugin should be loaded";
}

// Test: Plugin with run function should initialize successfully
TEST_F(PluginManagerTest, 069_InitPlugin_WithRun_ShouldSucceed) {
    create_test_config("test_config_run_init.yaml", "RunInitPlugin", "test065-run-init-uuid-ghi");


    TestPluginManager manager;
    bool load_result = manager.load_plugin(
        "../plugins/test_plugin_with_run" PLUGIN_EXT,
        "test_config_run_init.yaml",
        "test065-run-init-uuid-ghi");

    ASSERT_TRUE(load_result) << "Plugin should load successfully";

    bool init_result = manager.init_plugin("test065-run-init-uuid-ghi");

    // Clean up properly by finalizing the plugin
    bool finit_result = manager.finit_plugin("test065-run-init-uuid-ghi");

    cleanup_test_file("test_config_run_init.yaml");

    ASSERT_TRUE(init_result) << "Plugin with run function should initialize successfully";
    ASSERT_TRUE(finit_result) << "Plugin should finalize successfully";
    ASSERT_TRUE(manager.is_plugin_loaded("test065-run-init-uuid-ghi")) << "Plugin should remain loaded";
}

// Test: Plugin with run function should start and stop successfully
TEST_F(PluginManagerTest, 070_StartStopPlugin_WithRun_ShouldSucceed) {
    create_test_config("test_config_run_startstop.yaml", "RunStartStopPlugin", "test066-run-startstop-uuid-jkl");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test_plugin_with_run" PLUGIN_EXT, "test_config_run_startstop.yaml", "test066-run-startstop-uuid-jkl");
    manager.init_plugin("test066-run-startstop-uuid-jkl");

    bool start_result = manager.start_plugin("test066-run-startstop-uuid-jkl");

    // Clean up properly
    bool stop_result = manager.stop_plugin("test066-run-startstop-uuid-jkl");
    bool finit_result = manager.finit_plugin("test066-run-startstop-uuid-jkl");
    bool unload_result = manager.unload_plugin("test066-run-startstop-uuid-jkl");

    cleanup_test_file("test_config_run_startstop.yaml");

    ASSERT_TRUE(start_result) << "start_plugin should succeed for plugin with run function";
    ASSERT_TRUE(stop_result) << "stop_plugin should succeed";
    ASSERT_TRUE(finit_result) << "finit_plugin should succeed";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: Plugin with run function should have run capability
TEST_F(PluginManagerTest, 071_GetCapabilities_WithRun_ShouldIncludeRun) {
    create_test_config("test_config_run_caps.yaml", "RunCapsPlugin", "test067-run-caps-uuid-mno");

    TestPluginManager manager;
    manager.load_plugin("../plugins/test_plugin_with_run" PLUGIN_EXT, "test_config_run_caps.yaml", "test067-run-caps-uuid-mno");

    std::vector<std::string> capabilities = manager.get_plugin_capabilities("test067-run-caps-uuid-mno");

    // Clean up properly
    bool unload_result = manager.unload_plugin("test067-run-caps-uuid-mno");

    cleanup_test_file("test_config_run_caps.yaml");

    // Should include "run" capability
    ASSERT_TRUE(std::find(capabilities.begin(), capabilities.end(), "run") != capabilities.end())
        << "Plugin with run function should have 'run' capability";
    ASSERT_TRUE(unload_result) << "unload_plugin should succeed";
}

// Test: Plugin without run function should not have run capability
TEST_F(PluginManagerTest, 072_GetCapabilities_NoRun_ShouldNotIncludeRun) {
    create_test_config("test_config_no_run_caps.yaml", "NoRunCapsPlugin", "test068-no-run-caps-uuid-pqr");


    TestPluginManager manager;
    manager.load_plugin("../plugins/test_plugin_no_run" PLUGIN_EXT, "test_config_no_run_caps.yaml", "test068-no-run-caps-uuid-pqr");

    std::vector<std::string> capabilities = manager.get_plugin_capabilities("test068-no-run-caps-uuid-pqr");

    // Clean up properly
    bool unload_result = manager.unload_plugin("test068-no-run-caps-uuid-pqr");

    cleanup_test_file("test_config_no_run_caps.yaml");

    // Should not include "run" capability
    ASSERT_TRUE(std::find(capabilities.begin(), capabilities.end(), "run") == capabilities.end())
        << "Plugin without run function should not have 'run' capability";
    ASSERT_FALSE(unload_result) << "unload_plugin should now succeed";
}

// Test: Complete lifecycle with run function - load, init, start, stop, finit, unload
TEST_F(PluginManagerTest, 073_CompleteLifecycle_WithRun_ShouldSucceed) {
    create_test_config("test_config_run_lifecycle.yaml", "RunLifecyclePlugin", "test069-run-lifecycle-uuid-stu");

    TestPluginManager manager;
    // Load
    ASSERT_TRUE(manager.load_plugin("../plugins/test_plugin_with_run" PLUGIN_EXT, "test_config_run_lifecycle.yaml", "test069-run-lifecycle-uuid-stu"))
        << "Load should succeed";
    ASSERT_TRUE(manager.is_plugin_loaded("test069-run-lifecycle-uuid-stu")) << "Plugin should be loaded";

    // Init
    ASSERT_TRUE(manager.init_plugin("test069-run-lifecycle-uuid-stu")) << "Init should succeed";

    // Start
    ASSERT_TRUE(manager.start_plugin("test069-run-lifecycle-uuid-stu")) << "Start should succeed";

    // Stop
    ASSERT_TRUE(manager.stop_plugin("test069-run-lifecycle-uuid-stu")) << "Stop should succeed";

    // Finit
    ASSERT_TRUE(manager.finit_plugin("test069-run-lifecycle-uuid-stu")) << "Finit should succeed";

    // Unload
    ASSERT_TRUE(manager.unload_plugin("test069-run-lifecycle-uuid-stu")) << "Unload should succeed";
    ASSERT_FALSE(manager.is_plugin_loaded("test069-run-lifecycle-uuid-stu")) << "Plugin should be unloaded";

    cleanup_test_file("test_config_run_lifecycle.yaml");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}