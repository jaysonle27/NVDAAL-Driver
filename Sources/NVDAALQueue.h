/*
 * NVDAALQueue.h - Compute Command Queue (GPFIFO)
 *
 * Manages the ring buffer and doorbell registers for submitting
 * compute kernels to the Ada Lovelace hardware.
 */

#ifndef NVDAAL_QUEUE_H
#define NVDAAL_QUEUE_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "NVDAALRegs.h"

class NVDAALQueue : public OSObject {
    OSDeclareDefaultStructors(NVDAALQueue);

private:
    IOBufferMemoryDescriptor *ringBufferMem;
    volatile uint32_t *ringBuffer;
    uint32_t ringSize;
    
    uint32_t head; // Read by GPU
    uint32_t tail; // Written by CPU
    
    volatile uint32_t *doorbell; // Hardware register to signal "kick"
    
    IOLock *lock;

public:
    static NVDAALQueue* withSize(uint32_t size, volatile uint32_t *doorbellAddr);
    
    virtual bool init() override;
    virtual void free() override;

    // Command submission
    bool push(uint32_t cmd);
    void kick(); // Signal the GPU to process the queue
    
    // Helpers
    bool isFull() const;
    uint32_t getFreeSpace() const;
    
    // Physical address for the GPU to find the buffer
    uint64_t getPhysicalAddress() const;
};

#endif // NVDAAL_QUEUE_H
