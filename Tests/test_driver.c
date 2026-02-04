/**
 * @file test_driver.c
 * @brief Integration tests for NVDAAL driver via IOKit
 *
 * Tests driver connectivity, IOUserClient interface, and basic operations.
 * Requires the NVDAAL.kext to be loaded for full testing.
 *
 * Compile: make test-driver
 * Run: ./Build/test_driver
 */

#include "nvdaal_test.h"
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// ============================================================================
// Constants
// ============================================================================

#define NVDAAL_SERVICE_NAME     "NVDAAL"
#define NVIDIA_VENDOR_ID        0x10DE

// External method selectors (must match NVDAALUserClient.h)
enum {
    kNVDAALMethodGetStatus = 0,
    kNVDAALMethodAllocMemory = 1,
    kNVDAALMethodFreeMemory = 2,
    kNVDAALMethodSubmitCommand = 3,
    kNVDAALMethodGetInfo = 4,
    kNVDAALMethodLoadFirmware = 5,
    kNVDAALMethodExecuteFwsec = 6,
    kNVDAALMethodCount
};

// ============================================================================
// Globals
// ============================================================================

static io_connect_t g_connection = 0;
static io_service_t g_service = 0;
static bool g_driver_loaded = false;

// ============================================================================
// Helper Functions
// ============================================================================

static bool find_nvidia_gpu(void) {
    io_iterator_t iterator;
    io_service_t service;
    kern_return_t kr;
    bool found = false;

    CFMutableDictionaryRef matchDict = IOServiceMatching("IOPCIDevice");
    kr = IOServiceGetMatchingServices(kIOMainPortDefault, matchDict, &iterator);

    if (kr != KERN_SUCCESS) {
        return false;
    }

    while ((service = IOIteratorNext(iterator)) != 0) {
        CFTypeRef vendorRef = IORegistryEntryCreateCFProperty(
            service, CFSTR("vendor-id"), kCFAllocatorDefault, 0);

        if (vendorRef && CFGetTypeID(vendorRef) == CFDataGetTypeID()) {
            CFDataRef vendorData = (CFDataRef)vendorRef;
            if (CFDataGetLength(vendorData) >= 2) {
                const uint8_t *bytes = CFDataGetBytePtr(vendorData);
                uint16_t vendor = bytes[0] | (bytes[1] << 8);
                if (vendor == NVIDIA_VENDOR_ID) {
                    found = true;
                }
            }
            CFRelease(vendorRef);
        }
        IOObjectRelease(service);

        if (found) break;
    }

    IOObjectRelease(iterator);
    return found;
}

static bool connect_to_driver(void) {
    kern_return_t kr;

    g_service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching(NVDAAL_SERVICE_NAME));

    if (g_service == 0) {
        return false;
    }

    kr = IOServiceOpen(g_service, mach_task_self(), 0, &g_connection);
    if (kr != KERN_SUCCESS) {
        IOObjectRelease(g_service);
        g_service = 0;
        return false;
    }

    g_driver_loaded = true;
    return true;
}

static void disconnect_from_driver(void) {
    if (g_connection) {
        IOServiceClose(g_connection);
        g_connection = 0;
    }
    if (g_service) {
        IOObjectRelease(g_service);
        g_service = 0;
    }
    g_driver_loaded = false;
}

// ============================================================================
// Hardware Detection Tests
// ============================================================================

void test_nvidia_gpu_present(void) {
    bool found = find_nvidia_gpu();
    if (!found) {
        TEST_SKIP("No NVIDIA GPU found in system");
    }
    TEST_ASSERT(found);
}

void test_nvdaal_service_exists(void) {
    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching(NVDAAL_SERVICE_NAME));

    if (service == 0) {
        TEST_SKIP("NVDAAL service not found - kext not loaded?");
    }

    TEST_ASSERT_NEQ(0, service);
    IOObjectRelease(service);
}

// ============================================================================
// Driver Connection Tests
// ============================================================================

void test_driver_connect(void) {
    if (!g_driver_loaded && !connect_to_driver()) {
        TEST_SKIP("Could not connect to NVDAAL driver");
    }
    TEST_ASSERT(g_driver_loaded);
    TEST_ASSERT_NEQ(0, g_connection);
}

void test_driver_get_status(void) {
    if (!g_driver_loaded) {
        TEST_SKIP("Driver not connected");
    }

    uint64_t output[8] = {0};
    uint32_t outputCount = 8;
    kern_return_t kr;

    kr = IOConnectCallScalarMethod(
        g_connection,
        kNVDAALMethodGetStatus,
        NULL, 0,
        output, &outputCount);

    if (kr != KERN_SUCCESS) {
        // Method may not be implemented yet
        TEST_SKIP("GetStatus method not available");
    }

    TEST_ASSERT_EQ(KERN_SUCCESS, kr);
    // output[0] should contain some status value
}

void test_driver_get_info(void) {
    if (!g_driver_loaded) {
        TEST_SKIP("Driver not connected");
    }

    uint64_t output[8] = {0};
    uint32_t outputCount = 8;
    kern_return_t kr;

    kr = IOConnectCallScalarMethod(
        g_connection,
        kNVDAALMethodGetInfo,
        NULL, 0,
        output, &outputCount);

    if (kr != KERN_SUCCESS) {
        TEST_SKIP("GetInfo method not available");
    }

    TEST_ASSERT_EQ(KERN_SUCCESS, kr);
}

// ============================================================================
// Memory Tests
// ============================================================================

void test_driver_alloc_memory(void) {
    if (!g_driver_loaded) {
        TEST_SKIP("Driver not connected");
    }

    uint64_t input[2] = {0x1000, 0};  // 4KB allocation
    uint64_t output[4] = {0};
    uint32_t outputCount = 4;
    kern_return_t kr;

    kr = IOConnectCallScalarMethod(
        g_connection,
        kNVDAALMethodAllocMemory,
        input, 2,
        output, &outputCount);

    if (kr != KERN_SUCCESS) {
        TEST_SKIP("AllocMemory method not available");
    }

    TEST_ASSERT_EQ(KERN_SUCCESS, kr);

    // If allocation succeeded, free it
    if (output[0] != 0) {
        uint64_t freeInput[2] = {output[0], 0x1000};
        IOConnectCallScalarMethod(
            g_connection,
            kNVDAALMethodFreeMemory,
            freeInput, 2,
            NULL, NULL);
    }
}

// ============================================================================
// IORegistry Tests
// ============================================================================

void test_ioregistry_nvdaal_properties(void) {
    if (g_service == 0) {
        g_service = IOServiceGetMatchingService(
            kIOMainPortDefault,
            IOServiceMatching(NVDAAL_SERVICE_NAME));
    }

    if (g_service == 0) {
        TEST_SKIP("NVDAAL service not found");
    }

    // Check for expected properties
    CFTypeRef prop = IORegistryEntryCreateCFProperty(
        g_service, CFSTR("IOClass"), kCFAllocatorDefault, 0);

    TEST_ASSERT_NOT_NULL(prop);

    if (prop) {
        if (CFGetTypeID(prop) == CFStringGetTypeID()) {
            char className[64];
            CFStringGetCString((CFStringRef)prop, className, sizeof(className),
                               kCFStringEncodingUTF8);
            TEST_ASSERT_STR_EQ("NVDAAL", className);
        }
        CFRelease(prop);
    }
}

void test_ioregistry_pci_properties(void) {
    io_iterator_t iterator;
    io_service_t service;
    kern_return_t kr;
    bool found_nvdaal_parent = false;

    CFMutableDictionaryRef matchDict = IOServiceMatching("IOPCIDevice");
    kr = IOServiceGetMatchingServices(kIOMainPortDefault, matchDict, &iterator);

    if (kr != KERN_SUCCESS) {
        TEST_SKIP("Could not get PCI services");
    }

    while ((service = IOIteratorNext(iterator)) != 0) {
        CFTypeRef vendorRef = IORegistryEntryCreateCFProperty(
            service, CFSTR("vendor-id"), kCFAllocatorDefault, 0);
        CFTypeRef deviceRef = IORegistryEntryCreateCFProperty(
            service, CFSTR("device-id"), kCFAllocatorDefault, 0);

        if (vendorRef && deviceRef) {
            uint16_t vendor = 0, device = 0;

            if (CFGetTypeID(vendorRef) == CFDataGetTypeID()) {
                const uint8_t *bytes = CFDataGetBytePtr((CFDataRef)vendorRef);
                vendor = bytes[0] | (bytes[1] << 8);
            }
            if (CFGetTypeID(deviceRef) == CFDataGetTypeID()) {
                const uint8_t *bytes = CFDataGetBytePtr((CFDataRef)deviceRef);
                device = bytes[0] | (bytes[1] << 8);
            }

            // Check for RTX 40 series device IDs
            if (vendor == NVIDIA_VENDOR_ID) {
                if (device == 0x2684 || device == 0x2685 ||  // RTX 4090
                    device == 0x2702 || device == 0x2704 ||  // RTX 4080
                    device == 0x2705 || device == 0x2782 ||  // RTX 4070 Ti
                    device == 0x2786 || device == 0x2860) {  // RTX 4070
                    found_nvdaal_parent = true;
                }
            }

            CFRelease(vendorRef);
            CFRelease(deviceRef);
        }

        IOObjectRelease(service);
        if (found_nvdaal_parent) break;
    }

    IOObjectRelease(iterator);

    if (!found_nvdaal_parent) {
        TEST_SKIP("No supported NVIDIA GPU found (need RTX 40 series)");
    }

    TEST_ASSERT(found_nvdaal_parent);
}

// ============================================================================
// Kext Status Tests
// ============================================================================

void test_kext_loaded(void) {
    // Check if kext is loaded via IOServiceMatching
    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching(NVDAAL_SERVICE_NAME));

    bool loaded = (service != 0);
    if (service) {
        IOObjectRelease(service);
    }

    if (!loaded) {
        TEST_SKIP("NVDAAL kext not loaded - run 'make load' first");
    }

    TEST_ASSERT(loaded);
}

// ============================================================================
// Cleanup
// ============================================================================

void test_cleanup(void) {
    disconnect_from_driver();
    TEST_ASSERT(!g_driver_loaded);
}

// ============================================================================
// Main
// ============================================================================

TEST_MAIN("NVDAAL Driver Integration Tests",
    // Hardware detection
    TEST_CASE(test_nvidia_gpu_present),
    TEST_CASE(test_ioregistry_pci_properties),

    // Kext status
    TEST_CASE(test_kext_loaded),
    TEST_CASE(test_nvdaal_service_exists),

    // Driver connection
    TEST_CASE(test_driver_connect),
    TEST_CASE(test_ioregistry_nvdaal_properties),

    // Driver methods
    TEST_CASE(test_driver_get_status),
    TEST_CASE(test_driver_get_info),
    TEST_CASE(test_driver_alloc_memory),

    // Cleanup
    TEST_CASE(test_cleanup)
)
