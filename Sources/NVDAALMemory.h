/*
 * NVDAALMemory.h - VRAM and DMA Memory Manager
 *
 * Handles allocation of GPU memory (VRAM) via BAR1 aperture
 * and system memory (GTT) for DMA.
 */

#ifndef NVDAAL_MEMORY_H
#define NVDAAL_MEMORY_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>

class NVDAALMemory : public OSObject {
    OSDeclareDefaultStructors(NVDAALMemory);

private:
    IOPCIDevice *pciDevice;
    IOMemoryMap *bar1Map;
    
    uint64_t vramBase;
    uint64_t vramSize;
    uint64_t freeOffset; // Simple linear allocator pointer

    IOLock *lock;

public:
    static NVDAALMemory* withDevice(IOPCIDevice *dev, IOMemoryMap *bar1);
    
    virtual bool init() override;
    virtual void free() override;

    // Basic VRAM Allocation (Linear, non-freeing for prototype)
    uint64_t allocVram(size_t size);
    
    // Create a memory descriptor for a VRAM region (for mapping to user-space)
    IOMemoryDescriptor* createVramDescriptor(uint64_t offset, size_t size);

    // Helpers
    uint64_t getTotalVram() const { return vramSize; }
    uint64_t getFreeVram() const { return vramSize - freeOffset; }
};

#endif // NVDAAL_MEMORY_H
