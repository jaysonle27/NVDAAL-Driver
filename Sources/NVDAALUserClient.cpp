/*
 * NVDAALUserClient.cpp - IOUserClient Implementation
 */

#include "NVDAALUserClient.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define super IOUserClient

OSDefineMetaClassAndStructors(NVDAALUserClient, IOUserClient);

// =============================================================================
// Lifecycle
// ============================================================================n
bool NVDAALUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type, OSDictionary *properties) {
    if (!super::initWithTask(owningTask, securityID, type, properties)) {
        return false;
    }
    clientTask = owningTask;
    return true;
}

bool NVDAALUserClient::start(IOService *provider) {
    if (!super::start(provider)) {
        return false;
    }
    
    this->provider = OSDynamicCast(NVDAAL, provider);
    if (!this->provider) {
        return false;
    }

    return true;
}

void NVDAALUserClient::stop(IOService *provider) {
    super::stop(provider);
    this->provider = nullptr;
}

IOReturn NVDAALUserClient::clientClose(void) {
    terminate();
    return kIOReturnSuccess;
}

// ============================================================================n// External Methods
// ============================================================================n

IOReturn NVDAALUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
                                          IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) {
    switch (selector) {
        case kNVDAALMethodLoadFirmware:
            return methodLoadFirmware(arguments);
        case kNVDAALMethodAllocVram:
            return methodAllocVram(arguments);
        case kNVDAALMethodSubmitCommand:
            return methodSubmitCommand(arguments);
        default:
            return kIOReturnBadArgument;
    }
}

IOReturn NVDAALUserClient::methodAllocVram(IOExternalMethodArguments *args) {
    if (args->scalarInputCount != 1 || args->scalarOutputCount != 1) {
        return kIOReturnBadArgument;
    }

    size_t size = (size_t)args->scalarInput[0];
    uint64_t offset = provider->allocVram(size);

    if (offset == 0 && size > 0) return kIOReturnNoMemory;

    args->scalarOutput[0] = offset;
    return kIOReturnSuccess;
}

IOReturn NVDAALUserClient::methodSubmitCommand(IOExternalMethodArguments *args) {
    if (args->scalarInputCount != 1) {
        return kIOReturnBadArgument;
    }

    uint32_t cmd = (uint32_t)args->scalarInput[0];
    bool ok = provider->submitCommand(cmd);

    return ok ? kIOReturnSuccess : kIOReturnError;
}

IOReturn NVDAALUserClient::methodLoadFirmware(IOExternalMethodArguments *args) {
    // Expects:
    // Input[0]: Pointer to GSP firmware (user virtual address)
    // Input[1]: Size of GSP firmware
    
    if (args->scalarInputCount != 2) {
        return kIOReturnBadArgument;
    }

    mach_vm_address_t userPtr = (mach_vm_address_t)args->scalarInput[0];
    mach_vm_size_t size = (mach_vm_size_t)args->scalarInput[1];

    IOLog("NVDAALUserClient: LoadFirmware called. Ptr: 0x%llx, Size: %llu\n", userPtr, size);

    // Validate size (sanity check, e.g. < 256MB)
    if (size == 0 || size > 0x10000000) {
        IOLog("NVDAALUserClient: Invalid firmware size\n");
        return kIOReturnBadArgument;
    }

    // Create memory descriptor from user memory
    IOMemoryDescriptor *memDesc = IOMemoryDescriptor::withAddressRange(
        userPtr,
        size,
        kIODirectionOut,
        clientTask
    );

    if (!memDesc) {
        IOLog("NVDAALUserClient: Failed to create memory descriptor\n");
        return kIOReturnNoMemory;
    }

    // Wire down the memory (prepare)
    IOReturn ret = memDesc->prepare(kIODirectionOut);
    if (ret != kIOReturnSuccess) {
        IOLog("NVDAALUserClient: Failed to wire memory (0x%08x)\n", ret);
        memDesc->release();
        return ret;
    }

    // Get physical address/map kernel side
    // For GSP, we likely need the physical address to pass to the GPU via DMA
    // We can also map it to kernel virtual address if we need to parse it (ELF)
    
    IOMemoryMap *map = memDesc->map();
    if (!map) {
        IOLog("NVDAALUserClient: Failed to map memory to kernel\n");
        memDesc->complete(kIODirectionOut);
        memDesc->release();
        return kIOReturnVMError;
    }

    void *kernelAddr = (void *)map->getVirtualAddress();
    
    // Call driver to load firmware
    // Note: provider->loadGspFirmware needs to be implemented publicly in NVDAAL
    bool ok = provider->loadGspFirmware(kernelAddr, size);
    
    // Cleanup
    map->release();
    memDesc->complete(kIODirectionOut);
    memDesc->release();

    return ok ? kIOReturnSuccess : kIOReturnError;
}
