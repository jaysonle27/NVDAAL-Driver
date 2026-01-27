/*
 * NVDAALMemory.cpp - VRAM Manager Implementation
 */

#include "NVDAALMemory.h"
#include <IOKit/IOLib.h>

#define super OSObject

OSDefineMetaClassAndStructors(NVDAALMemory, OSObject);

NVDAALMemory* NVDAALMemory::withDevice(IOPCIDevice *dev, IOMemoryMap *bar1) {
    NVDAALMemory *inst = new NVDAALMemory;
    if (inst) {
        inst->pciDevice = dev;
        inst->bar1Map = bar1;
        if (!inst->init()) {
            inst->release();
            return nullptr;
        }
    }
    return inst;
}

bool NVDAALMemory::init() {
    if (!super::init()) return false;
    
    if (!pciDevice || !bar1Map) return false;

    vramBase = bar1Map->getVirtualAddress();
    vramSize = bar1Map->getLength();
    freeOffset = 0;
    
    lock = IOLockAlloc();
    if (!lock) return false;

    IOLog("NVDAAL-Mem: Initialized VRAM Manager. Total: %llu MB\n", vramSize / (1024 * 1024));
    
    return true;
}

void NVDAALMemory::free() {
    if (lock) {
        IOLockFree(lock);
    }
    super::free();
}

uint64_t NVDAALMemory::allocVram(size_t size) {
    IOLockLock(lock);
    
    // Simple 4KB alignment
    size_t alignedSize = (size + 4095ULL) & ~4095ULL;
    
    if (freeOffset + alignedSize > vramSize) {
        IOLockUnlock(lock);
        IOLog("NVDAAL-Mem: OOM! Requested %lu, Available %llu\n", size, vramSize - freeOffset);
        return 0;
    }
    
    uint64_t allocatedOffset = freeOffset;
    freeOffset += alignedSize;
    
    IOLockUnlock(lock);
    
    // Zero out the memory (security/cleanliness)
    // Note: Writing to BAR1 can be slow, but essential for safety
    memset((void *)(vramBase + allocatedOffset), 0, alignedSize);
    
    return allocatedOffset;
}

IOMemoryDescriptor* NVDAALMemory::createVramDescriptor(uint64_t offset, size_t size) {
    if (offset + size > vramSize) return nullptr;
    
    // Create a descriptor pointing to the physical/virtual aperture
    // Since BAR1 is mapped kernel side, we can use withAddressRange on the virtual address
    // This allows IOUserClient to map it later.
    
    return IOMemoryDescriptor::withAddressRange(
        vramBase + offset,
        size,
        kIODirectionInOut,
        kernel_task
    );
}
