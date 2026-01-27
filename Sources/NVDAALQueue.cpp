/*
 * NVDAALQueue.cpp - Compute Command Queue Implementation
 */

#include "NVDAALQueue.h"
#include <IOKit/IOLib.h>

#define super OSObject

OSDefineMetaClassAndStructors(NVDAALQueue, OSObject);

NVDAALQueue* NVDAALQueue::withSize(uint32_t size, volatile uint32_t *doorbellAddr) {
    NVDAALQueue *inst = new NVDAALQueue;
    if (inst) {
        inst->ringSize = size;
        inst->doorbell = doorbellAddr;
        if (!inst->init()) {
            inst->release();
            return nullptr;
        }
    }
    return inst;
}

bool NVDAALQueue::init() {
    if (!super::init()) return false;
    
    // Allocate physically contiguous ring buffer for the GPU
    ringBufferMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task,
        kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        ringSize * sizeof(uint32_t),
        0xFFFFFFFFFFFFULL
    );

    if (!ringBufferMem) return false;

    if (ringBufferMem->prepare() != kIOReturnSuccess) {
        ringBufferMem->release();
        return false;
    }

    ringBuffer = (volatile uint32_t *)ringBufferMem->getBytesNoCopy();
    memset((void *)ringBuffer, 0, ringSize * sizeof(uint32_t));

    head = 0;
    tail = 0;
    
    lock = IOLockAlloc();
    if (!lock) return false;

    IOLog("NVDAAL-Queue: Command Queue initialized (%u entries)\n", ringSize);
    
    return true;
}

void NVDAALQueue::free() {
    if (lock) IOLockFree(lock);
    if (ringBufferMem) {
        ringBufferMem->complete();
        ringBufferMem->release();
    }
    super::free();
}

bool NVDAALQueue::push(uint32_t cmd) {
    IOLockLock(lock);
    
    uint32_t nextTail = (tail + 1) % ringSize;
    if (nextTail == head) {
        IOLockUnlock(lock);
        IOLog("NVDAAL-Queue: OVERFLOW!\n");
        return false;
    }
    
    ringBuffer[tail] = cmd;
    tail = nextTail;
    
    IOLockUnlock(lock);
    return true;
}

void NVDAALQueue::kick() {
    if (!doorbell) return;
    
    // In NVIDIA architecture, writing the tail pointer to the doorbell
    // register tells the GPU there is new work to do.
    *doorbell = tail;
    
    // Ensure memory barrier so the GPU sees the buffer update before the doorbell
    __asm__ __volatile__ ("mfence" ::: "memory");
}

uint64_t NVDAALQueue::getPhysicalAddress() const {
    return ringBufferMem ? ringBufferMem->getPhysicalSegment(0, nullptr) : 0;
}

bool NVDAALQueue::isFull() const {
    return ((tail + 1) % ringSize) == head;
}

uint32_t NVDAALQueue::getFreeSpace() const {
    if (tail >= head) {
        return ringSize - (tail - head) - 1;
    } else {
        return head - tail - 1;
    }
}
