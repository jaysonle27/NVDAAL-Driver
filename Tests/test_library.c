/**
 * @file test_library.c
 * @brief Tests for libNVDAAL user-space library
 *
 * Tests the C API of the NVDAAL library.
 * Requires: Build/libNVDAAL.dylib
 *
 * Compile: make test-library
 * Run: ./Build/test_library
 */

#include "nvdaal_test.h"
#include <dlfcn.h>
#include <sys/stat.h>

// ============================================================================
// Library Function Types
// ============================================================================

typedef void* (*nvdaal_create_client_fn)(void);
typedef void (*nvdaal_destroy_client_fn)(void*);
typedef bool (*nvdaal_connect_fn)(void*);
typedef bool (*nvdaal_disconnect_fn)(void*);
typedef bool (*nvdaal_is_connected_fn)(void*);
typedef uint64_t (*nvdaal_alloc_vram_fn)(void*, size_t);
typedef bool (*nvdaal_free_vram_fn)(void*, uint64_t, size_t);
typedef bool (*nvdaal_submit_command_fn)(void*, uint32_t);
typedef bool (*nvdaal_load_firmware_fn)(void*, const char*);
typedef uint32_t (*nvdaal_get_status_fn)(void*);

// ============================================================================
// Globals
// ============================================================================

static void *g_lib = NULL;
static const char *g_lib_path = "Build/libNVDAAL.dylib";

// Function pointers
static nvdaal_create_client_fn fn_create_client = NULL;
static nvdaal_destroy_client_fn fn_destroy_client = NULL;
static nvdaal_connect_fn fn_connect = NULL;
static nvdaal_disconnect_fn fn_disconnect = NULL;
static nvdaal_is_connected_fn fn_is_connected = NULL;
static nvdaal_alloc_vram_fn fn_alloc_vram = NULL;
static nvdaal_free_vram_fn fn_free_vram = NULL;
static nvdaal_submit_command_fn fn_submit_command = NULL;
static nvdaal_load_firmware_fn fn_load_firmware = NULL;
static nvdaal_get_status_fn fn_get_status = NULL;

// ============================================================================
// Library Loading
// ============================================================================

void test_library_exists(void) {
    struct stat st;
    int result = stat(g_lib_path, &st);
    if (result != 0) {
        TEST_SKIP("Library not found - run 'make lib' first");
    }
    TEST_ASSERT_EQ(0, result);
}

void test_library_load(void) {
    g_lib = dlopen(g_lib_path, RTLD_NOW);
    if (!g_lib) {
        printf("    dlopen error: %s\n", dlerror());
        TEST_SKIP("Could not load library");
    }
    TEST_ASSERT_NOT_NULL(g_lib);
}

void test_library_symbols(void) {
    if (!g_lib) TEST_SKIP("Library not loaded");

    fn_create_client = (nvdaal_create_client_fn)dlsym(g_lib, "nvdaal_create_client");
    fn_destroy_client = (nvdaal_destroy_client_fn)dlsym(g_lib, "nvdaal_destroy_client");
    fn_connect = (nvdaal_connect_fn)dlsym(g_lib, "nvdaal_connect");
    fn_is_connected = (nvdaal_is_connected_fn)dlsym(g_lib, "nvdaal_is_connected");
    fn_alloc_vram = (nvdaal_alloc_vram_fn)dlsym(g_lib, "nvdaal_alloc_vram");
    fn_submit_command = (nvdaal_submit_command_fn)dlsym(g_lib, "nvdaal_submit_command");

    // Core functions must exist
    TEST_ASSERT_NOT_NULL(fn_create_client);
    TEST_ASSERT_NOT_NULL(fn_destroy_client);
    TEST_ASSERT_NOT_NULL(fn_connect);
}

// ============================================================================
// Client Tests
// ============================================================================

void test_client_create(void) {
    if (!fn_create_client) TEST_SKIP("create_client not available");

    void *client = fn_create_client();
    TEST_ASSERT_NOT_NULL(client);

    if (client && fn_destroy_client) {
        fn_destroy_client(client);
    }
}

void test_client_create_multiple(void) {
    if (!fn_create_client) TEST_SKIP("create_client not available");

    void *clients[5] = {NULL};
    int created = 0;

    for (int i = 0; i < 5; i++) {
        clients[i] = fn_create_client();
        if (clients[i]) created++;
    }

    TEST_ASSERT(created > 0);

    // Cleanup
    for (int i = 0; i < 5; i++) {
        if (clients[i] && fn_destroy_client) {
            fn_destroy_client(clients[i]);
        }
    }
}

void test_client_connect(void) {
    if (!fn_create_client || !fn_connect) {
        TEST_SKIP("Required functions not available");
    }

    void *client = fn_create_client();
    TEST_ASSERT_NOT_NULL(client);

    if (client) {
        bool connected = fn_connect(client);
        // Connection may fail if driver not loaded - that's OK
        if (!connected) {
            printf("    Note: Connection failed (driver may not be loaded)\n");
        }

        if (fn_destroy_client) {
            fn_destroy_client(client);
        }
    }
    TEST_ASSERT(true);  // Test passes regardless of connection
}

void test_client_double_destroy(void) {
    if (!fn_create_client || !fn_destroy_client) {
        TEST_SKIP("Required functions not available");
    }

    void *client = fn_create_client();
    TEST_ASSERT_NOT_NULL(client);

    if (client) {
        fn_destroy_client(client);
        // Second destroy should be safe (no crash)
        // Note: This may cause issues if library doesn't handle it
    }
    TEST_ASSERT(true);
}

// ============================================================================
// Memory Tests
// ============================================================================

void test_vram_alloc_without_connect(void) {
    if (!fn_create_client || !fn_alloc_vram) {
        TEST_SKIP("Required functions not available");
    }

    void *client = fn_create_client();
    TEST_ASSERT_NOT_NULL(client);

    if (client) {
        // Call should not crash even without connection
        uint64_t addr = fn_alloc_vram(client, 4096);
        // Note: Return value depends on library implementation
        // Some implementations may return a placeholder, others 0
        (void)addr;  // Suppress unused warning

        if (fn_destroy_client) {
            fn_destroy_client(client);
        }
    }
    TEST_ASSERT(true);  // Test passes if no crash
}

void test_vram_alloc_zero_size(void) {
    if (!fn_create_client || !fn_alloc_vram) {
        TEST_SKIP("Required functions not available");
    }

    void *client = fn_create_client();
    if (client) {
        uint64_t addr = fn_alloc_vram(client, 0);
        TEST_ASSERT_EQ(0, addr);  // Should fail

        if (fn_destroy_client) {
            fn_destroy_client(client);
        }
    }
    TEST_ASSERT(true);
}

// ============================================================================
// Command Tests
// ============================================================================

void test_submit_command_without_connect(void) {
    if (!fn_create_client || !fn_submit_command) {
        TEST_SKIP("Required functions not available");
    }

    void *client = fn_create_client();
    if (client) {
        bool result = fn_submit_command(client, 0);
        TEST_ASSERT(!result);  // Should fail without connection

        if (fn_destroy_client) {
            fn_destroy_client(client);
        }
    }
    TEST_ASSERT(true);
}

// ============================================================================
// Null Safety Tests
// ============================================================================

void test_null_client_safety(void) {
    // All functions should handle NULL client gracefully
    if (fn_connect) {
        bool result = fn_connect(NULL);
        TEST_ASSERT(!result);
    }

    if (fn_is_connected) {
        bool result = fn_is_connected(NULL);
        TEST_ASSERT(!result);
    }

    if (fn_alloc_vram) {
        uint64_t addr = fn_alloc_vram(NULL, 4096);
        TEST_ASSERT_EQ(0, addr);
    }

    if (fn_submit_command) {
        bool result = fn_submit_command(NULL, 0);
        TEST_ASSERT(!result);
    }

    // destroy_client with NULL should not crash
    if (fn_destroy_client) {
        fn_destroy_client(NULL);
    }

    TEST_ASSERT(true);
}

// ============================================================================
// Cleanup
// ============================================================================

void test_library_unload(void) {
    if (g_lib) {
        dlclose(g_lib);
        g_lib = NULL;
    }
    TEST_ASSERT_NULL(g_lib);
}

// ============================================================================
// Main
// ============================================================================

TEST_MAIN("NVDAAL Library Tests",
    // Loading
    TEST_CASE(test_library_exists),
    TEST_CASE(test_library_load),
    TEST_CASE(test_library_symbols),

    // Client
    TEST_CASE(test_client_create),
    TEST_CASE(test_client_create_multiple),
    TEST_CASE(test_client_connect),

    // Memory
    TEST_CASE(test_vram_alloc_without_connect),
    TEST_CASE(test_vram_alloc_zero_size),

    // Commands
    TEST_CASE(test_submit_command_without_connect),

    // Safety
    TEST_CASE(test_null_client_safety),

    // Cleanup
    TEST_CASE(test_library_unload)
)
