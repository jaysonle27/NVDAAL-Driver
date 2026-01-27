/*
 * NVDAALDisplay.cpp - Graphics Properties Injection
 */

#include "NVDAALDisplay.h"
#include <IOKit/IOLib.h>

#define super IOService

OSDefineMetaClassAndStructors(NVDAALDisplay, IOService);

NVDAALDisplay* NVDAALDisplay::withDevice(IOPCIDevice *dev) {
    NVDAALDisplay *inst = new NVDAALDisplay;
    if (inst) {
        inst->pciDevice = dev;
        if (!inst->init()) {
            inst->release();
            return nullptr;
        }
    }
    return inst;
}

bool NVDAALDisplay::init() {
    return super::init();
}

bool NVDAALDisplay::start(IOService *provider) {
    if (!super::start(provider)) return false;
    
    IOLog("NVDAAL-Display: Starting Fake Display Engine...\n");
    injectGraphcisProperties();
    
    registerService();
    return true;
}

void NVDAALDisplay::stop(IOService *provider) {
    super::stop(provider);
}

void NVDAALDisplay::injectGraphcisProperties() {
    if (!pciDevice) return;

    // 1. Set Device Type (The magic string)
    pciDevice->setProperty("device_type", "NVDA,Parent");
    pciDevice->setProperty("model", "NVIDIA GeForce RTX 4090 Prototype");
    
    // 2. VRAM Size (24GB = 25769803776 bytes)
    uint64_t vramSize = 24ULL * 1024 * 1024 * 1024;
    pciDevice->setProperty("VRAM,totalsize", vramSize, 64);
    pciDevice->setProperty("VRAM,memsize", vramSize, 64);

    // 3. Metal Support (The bluff)
    // Telling macOS "Yes, I support Metal" might cause WindowServer to crash 
    // if we don't implement the UserClient calls, but let's try!
    pciDevice->setProperty("MetalPluginName", "NVDAALMetal"); 
    pciDevice->setProperty("MetalPluginClassName", "NVDAALMetalDevice");
    
    // 4. Compatibility
    pciDevice->setProperty("IOProbeScore", 5000, 32);
    
    IOLog("NVDAAL-Display: Injected graphics properties into IORegistry.\n");
}
