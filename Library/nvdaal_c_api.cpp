/*
 * nvdaal_c_api.cpp - C-compatible wrapper for libNVDAAL
 *
 * This allows Python (via ctypes) or other languages to use the driver.
 */

#include "libNVDAAL.h"

extern "C" {

void* nvdaal_create_client() {
    return new nvdaal::Client();
}

void nvdaal_destroy_client(void* client) {
    delete static_cast<nvdaal::Client*>(client);
}

bool nvdaal_connect(void* client) {
    return static_cast<nvdaal::Client*>(client)->connect();
}

uint64_t nvdaal_alloc_vram(void* client, size_t size) {
    return static_cast<nvdaal::Client*>(client)->allocVram(size);
}

bool nvdaal_submit_command(void* client, uint32_t cmd) {
    return static_cast<nvdaal::Client*>(client)->submitCommand(cmd);
}

bool nvdaal_load_firmware(void* client, const char* path) {
    return static_cast<nvdaal::Client*>(client)->loadFirmware(path);
}

}
