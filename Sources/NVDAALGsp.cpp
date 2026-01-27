/*
 * NVDAALGsp.cpp - GSP Controller Implementation
 *
 * Implements GSP initialization and RPC communication for Ada Lovelace GPUs.
 * Based on TinyGPU/tinygrad but rewritten for IOKit/macOS.
 */

#include "NVDAALGsp.h"
#include <libkern/libkern.h>
#include <libkern/OSByteOrder.h>

// ============================================================================
// Constructor / Destructor
// ============================================================================

NVDAALGsp::NVDAALGsp(void) {
    pciDevice = nullptr;
    mmioBase = nullptr;
    initialized = false;
    gspReady = false;
    rpcSeqNum = 0;

    cmdQueueMem = nullptr;
    statQueueMem = nullptr;
    firmwareMem = nullptr;
    bootloaderMem = nullptr;
    wprMetaMem = nullptr;
    radix3Mem = nullptr;

    cmdQueue = nullptr;
    statQueue = nullptr;
    cmdQueueHead = 0;
    cmdQueueTail = 0;
    statQueueHead = 0;
    statQueueTail = 0;
}

NVDAALGsp::~NVDAALGsp(void) {
    free();
}

// ============================================================================
// Initialization
// ============================================================================

bool NVDAALGsp::init(IOPCIDevice *device, volatile uint32_t *mmio) {
    if (initialized) {
        IOLog("NVDAAL-GSP: Already initialized\n");
        return false;
    }

    pciDevice = device;
    mmioBase = mmio;

    // Read chip info
    uint32_t boot0 = readReg(NV_PMC_BOOT_0);
    uint32_t arch = (boot0 >> 20) & 0x1F;

    IOLog("NVDAAL-GSP: Chip architecture: 0x%02x\n", arch);

    if (arch != NV_CHIP_ARCH_ADA) {
        IOLog("NVDAAL-GSP: Warning - not Ada Lovelace (0x%02x), expected 0x19\n", arch);
    }

    // Check WPR2 status
    uint32_t wpr2Hi = readReg(NV_PFB_PRI_MMU_WPR2_ADDR_HI);
    if (NV_PFB_WPR2_ENABLED(wpr2Hi)) {
        IOLog("NVDAAL-GSP: WPR2 already active - need PCI reset\n");
        // TODO: Implement PCI reset
    }

    // Allocate command queue (host -> GSP)
    if (!allocDmaBuffer(&cmdQueueMem, QUEUE_SIZE, &cmdQueuePhys)) {
        IOLog("NVDAAL-GSP: Failed to allocate command queue\n");
        return false;
    }
    cmdQueue = (volatile uint8_t *)cmdQueueMem->getBytesNoCopy();
    memset((void *)cmdQueue, 0, QUEUE_SIZE);

    // Allocate status queue (GSP -> host)
    if (!allocDmaBuffer(&statQueueMem, QUEUE_SIZE, &statQueuePhys)) {
        IOLog("NVDAAL-GSP: Failed to allocate status queue\n");
        free();
        return false;
    }
    statQueue = (volatile uint8_t *)statQueueMem->getBytesNoCopy();
    memset((void *)statQueue, 0, QUEUE_SIZE);

    // Allocate WPR metadata buffer
    if (!allocDmaBuffer(&wprMetaMem, PAGE_SIZE, &wprMetaPhys)) {
        IOLog("NVDAAL-GSP: Failed to allocate WPR meta\n");
        free();
        return false;
    }

    IOLog("NVDAAL-GSP: Queues allocated\n");
    IOLog("NVDAAL-GSP:   cmdQueue  @ 0x%llx\n", cmdQueuePhys);
    IOLog("NVDAAL-GSP:   statQueue @ 0x%llx\n", statQueuePhys);

    initialized = true;
    return true;
}

void NVDAALGsp::free(void) {
    gspReady = false;
    initialized = false;

    freeDmaBuffer(&radix3Mem);
    freeDmaBuffer(&wprMetaMem);
    freeDmaBuffer(&bootloaderMem);
    freeDmaBuffer(&firmwareMem);
    freeDmaBuffer(&statQueueMem);
    freeDmaBuffer(&cmdQueueMem);

    cmdQueue = nullptr;
    statQueue = nullptr;
    mmioBase = nullptr;
    pciDevice = nullptr;
}

// ============================================================================
// DMA Buffer Management
// ============================================================================

bool NVDAALGsp::allocDmaBuffer(IOBufferMemoryDescriptor **desc, size_t size, uint64_t *physAddr) {
    // Allocate physically contiguous, DMA-able memory
    *desc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task,
        kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        size,
        0xFFFFFFFFFFFFULL  // 48-bit physical address mask
    );

    if (!*desc) {
        return false;
    }

    IOReturn ret = (*desc)->prepare();
    if (ret != kIOReturnSuccess) {
        (*desc)->release();
        *desc = nullptr;
        return false;
    }

    // Get physical address
    *physAddr = (*desc)->getPhysicalSegment(0, nullptr);

    return true;
}

void NVDAALGsp::freeDmaBuffer(IOBufferMemoryDescriptor **desc) {
    if (*desc) {
        (*desc)->complete();
        (*desc)->release();
        *desc = nullptr;
    }
}

// ============================================================================
// Firmware Loading
// ============================================================================

bool NVDAALGsp::loadFirmware(const char *firmwarePath) {
    // TODO: Load firmware from file
    // For now, firmware should be loaded externally and passed to loadBootloader
    IOLog("NVDAAL-GSP: loadFirmware not implemented - use loadBootloader\n");
    return false;
}

bool NVDAALGsp::loadBootloader(const void *data, size_t size) {
    if (!initialized) {
        return false;
    }

    // Allocate memory for bootloader
    if (!allocDmaBuffer(&bootloaderMem, size, &bootloaderPhys)) {
        IOLog("NVDAAL-GSP: Failed to allocate bootloader memory\n");
        return false;
    }

    // Copy bootloader data
    memcpy(bootloaderMem->getBytesNoCopy(), data, size);

    IOLog("NVDAAL-GSP: Bootloader loaded (%lu bytes) @ 0x%llx\n",
          (unsigned long)size, bootloaderPhys);

    return true;
}

bool NVDAALGsp::parseElfFirmware(const void *data, size_t size) {
    // TODO: Parse ELF and extract .fwimage section
    // For Ada, also need .fwsignature_ad10x

    const uint8_t *elf = (const uint8_t *)data;

    // Check ELF magic
    if (elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F') {
        IOLog("NVDAAL-GSP: Invalid ELF magic\n");
        return false;
    }

    // TODO: Full ELF parsing
    IOLog("NVDAAL-GSP: ELF parsing not fully implemented\n");

    return true;
}

bool NVDAALGsp::buildRadix3PageTable(const void *firmware, size_t size) {
    // Radix3 is a 4-level page table for GSP firmware access
    // Level 0: 1 page (root)
    // Level 1-3: Index pages pointing to firmware pages

    size_t numPages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t level2Entries = (numPages + 511) / 512;
    size_t level1Entries = (level2Entries + 511) / 512;

    size_t totalPages = 1 + level1Entries + level2Entries + numPages;
    size_t radix3Size = totalPages * PAGE_SIZE;

    if (!allocDmaBuffer(&radix3Mem, radix3Size, &radix3Phys)) {
        IOLog("NVDAAL-GSP: Failed to allocate radix3 table\n");
        return false;
    }

    // TODO: Build actual page table structure
    IOLog("NVDAAL-GSP: Radix3 table allocated (%lu pages) @ 0x%llx\n",
          (unsigned long)totalPages, radix3Phys);

    return true;
}

// ============================================================================
// WPR Metadata Setup
// ============================================================================

bool NVDAALGsp::setupWprMeta(void) {
    if (!wprMetaMem) {
        return false;
    }

    GspFwWprMeta *meta = (GspFwWprMeta *)wprMetaMem->getBytesNoCopy();
    memset(meta, 0, sizeof(GspFwWprMeta));

    meta->magic = 0x57505232;  // "WPR2"

    meta->sysmemAddrOfBootloader = bootloaderPhys;
    meta->sizeOfBootloader = bootloaderMem ? bootloaderMem->getLength() : 0;

    meta->sysmemAddrOfRadix3Elf = radix3Phys;
    meta->sizeOfRadix3Elf = radix3Mem ? radix3Mem->getLength() : 0;

    meta->gspFwHeapSize = GSP_HEAP_SIZE;
    meta->frtsSize = FRTS_SIZE;

    // TODO: Fill in remaining fields

    IOLog("NVDAAL-GSP: WPR metadata configured\n");

    return true;
}

// ============================================================================
// Boot Sequence
// ============================================================================

bool NVDAALGsp::boot(void) {
    if (!initialized) {
        IOLog("NVDAAL-GSP: Not initialized\n");
        return false;
    }

    IOLog("NVDAAL-GSP: Starting boot sequence...\n");

    // Step 1: Reset FALCON
    if (!resetFalcon()) {
        IOLog("NVDAAL-GSP: FALCON reset failed\n");
        return false;
    }

    // Step 2: Execute FWSEC (from VBIOS)
    // TODO: This requires VBIOS parsing
    // if (!executeFwsec()) {
    //     return false;
    // }

    // Step 3: Setup WPR metadata
    if (!setupWprMeta()) {
        IOLog("NVDAAL-GSP: WPR meta setup failed\n");
        return false;
    }

    // Step 4: Start RISC-V core
    if (!startRiscv()) {
        IOLog("NVDAAL-GSP: RISC-V start failed\n");
        return false;
    }

    IOLog("NVDAAL-GSP: Boot sequence initiated\n");

    return true;
}

bool NVDAALGsp::resetFalcon(void) {
    // Reset the FALCON/GSP processor
    IOLog("NVDAAL-GSP: Resetting FALCON...\n");

    // Write reset sequence
    writeReg(NV_PGSP_FALCON_CPUCTL, 0);
    IODelay(100);  // 100us delay

    // Check if halted
    uint32_t cpuctl = readReg(NV_PGSP_FALCON_CPUCTL);
    if (!(cpuctl & FALCON_CPUCTL_HALTED)) {
        IOLog("NVDAAL-GSP: FALCON not halted after reset\n");
        // Continue anyway for now
    }

    return true;
}

bool NVDAALGsp::startRiscv(void) {
    IOLog("NVDAAL-GSP: Starting RISC-V core...\n");

    // Configure boot config register with WPR meta address
    uint32_t bcrAddr = (uint32_t)(wprMetaPhys >> 8);  // 256-byte aligned
    writeReg(NV_PRISCV_RISCV_BCR_DMEM_ADDR, bcrAddr);
    writeReg(NV_PRISCV_RISCV_BCR_CTRL, NV_PRISCV_RISCV_BCR_CTRL_VALID | bcrAddr);

    // Start the core
    writeReg(NV_PRISCV_RISCV_CPUCTL, NV_PRISCV_CPUCTL_START);

    // Wait for core to become active
    for (int i = 0; i < 100; i++) {
        uint32_t status = readReg(NV_PRISCV_RISCV_CPUCTL);
        if (status & NV_PRISCV_CPUCTL_ACTIVE) {
            IOLog("NVDAAL-GSP: RISC-V core active\n");
            return true;
        }
        IODelay(1000);  // 1ms delay
    }

    IOLog("NVDAAL-GSP: RISC-V core failed to start\n");
    return false;
}

bool NVDAALGsp::waitForInitDone(uint32_t timeoutMs) {
    IOLog("NVDAAL-GSP: Waiting for GSP_INIT_DONE...\n");

    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        // Check mailbox for init done signal
        uint32_t mailbox = readReg(NV_PGSP_FALCON_MAILBOX0);
        if (mailbox == NV_VGPU_MSG_EVENT_GSP_INIT_DONE) {
            IOLog("NVDAAL-GSP: GSP_INIT_DONE received!\n");
            gspReady = true;
            return true;
        }

        // Also check status queue for init done event
        // TODO: Implement proper queue polling

        IODelay(10000);  // 10ms delay
        elapsed += 10;
    }

    IOLog("NVDAAL-GSP: Timeout waiting for GSP_INIT_DONE\n");
    return false;
}

uint32_t NVDAALGsp::getBootStatus(void) const {
    if (!mmioBase) return 0xFFFFFFFF;
    return mmioBase[NV_PRISCV_RISCV_BR_RETCODE / 4];
}

// ============================================================================
// RPC Communication
// ============================================================================

uint32_t NVDAALGsp::calcChecksum(const void *data, size_t size) {
    // Simple CRC32-like checksum
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *bytes = (const uint8_t *)data;

    for (size_t i = 0; i < size; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}

bool NVDAALGsp::enqueueCommand(const void *msg, size_t size) {
    if (!cmdQueue) return false;

    // Calculate total element size (header + payload)
    size_t elemSize = sizeof(GspQueueElement) + size;
    size_t alignedSize = (elemSize + 0xFF) & ~0xFF;  // 256-byte aligned

    // Check for queue space
    uint32_t head = cmdQueueHead;
    uint32_t tail = cmdQueueTail;
    size_t freeSpace = (tail >= head) ? (QUEUE_SIZE - tail + head) : (head - tail);

    if (freeSpace < alignedSize) {
        IOLog("NVDAAL-GSP: Command queue full\n");
        return false;
    }

    // Write queue element
    GspQueueElement *elem = (GspQueueElement *)(cmdQueue + tail);
    elem->seqNum = rpcSeqNum++;
    elem->elemCount = (uint32_t)((alignedSize + 0xFFF) / 0x1000);  // 4KB pages
    elem->reserved = 0;

    // Copy message payload
    memcpy(elem->data, msg, size);

    // Calculate checksum
    elem->checkSum = calcChecksum(elem->data, size);

    // Update tail
    cmdQueueTail = (tail + alignedSize) % QUEUE_SIZE;

    // Update hardware queue pointer
    writeReg(NV_PGSP_QUEUE_TAIL(GSP_CMDQ_IDX), cmdQueueTail);

    return true;
}

bool NVDAALGsp::sendRpc(uint32_t function, const void *params, size_t paramsSize) {
    if (!gspReady && function != NV_VGPU_MSG_FUNCTION_GSP_SET_SYSTEM_INFO) {
        IOLog("NVDAAL-GSP: GSP not ready\n");
        return false;
    }

    // Build RPC message
    size_t msgSize = sizeof(NvRpcMessageHeader) + paramsSize;
    uint8_t *msgBuf = (uint8_t *)IOMalloc(msgSize);
    if (!msgBuf) return false;

    NvRpcMessageHeader *header = (NvRpcMessageHeader *)msgBuf;
    header->signature = NV_VGPU_MSG_SIGNATURE_VALID;
    header->headerVersion = (3 << 24);
    header->rpcResult = 0;
    header->rpcResultPriv = 0;
    header->function = function;
    header->length = (uint32_t)msgSize;

    if (params && paramsSize > 0) {
        memcpy(msgBuf + sizeof(NvRpcMessageHeader), params, paramsSize);
    }

    // Enqueue
    bool ok = enqueueCommand(msgBuf, msgSize);

    IOFree(msgBuf, msgSize);

    if (ok) {
        IOLog("NVDAAL-GSP: RPC 0x%02x sent\n", function);
    }

    return ok;
}

bool NVDAALGsp::waitRpcResponse(uint32_t function, void *response, size_t responseSize, uint32_t timeoutMs) {
    // TODO: Implement proper status queue polling
    IOLog("NVDAAL-GSP: waitRpcResponse not fully implemented\n");
    return false;
}

bool NVDAALGsp::sendSystemInfo(void) {
    GspSystemInfo info;
    memset(&info, 0, sizeof(info));

    // Get PCI info from device
    if (pciDevice) {
        info.pciVendorId = pciDevice->configRead16(0x00);
        info.pciDeviceId = pciDevice->configRead16(0x02);
        info.pciSubVendorId = pciDevice->configRead16(0x2C);
        info.pciSubDeviceId = pciDevice->configRead16(0x2E);
        info.pciRevisionId = pciDevice->configRead8(0x08);

        // Get BAR addresses
        info.gpuPhysAddr = pciDevice->configRead32(0x10) & 0xFFFFFFF0;
        info.fbPhysAddr = pciDevice->configRead32(0x14) & 0xFFFFFFF0;
    }

    IOLog("NVDAAL-GSP: Sending system info (device 0x%04x)\n", info.pciDeviceId);

    return sendRpc(NV_VGPU_MSG_FUNCTION_GSP_SET_SYSTEM_INFO, &info, sizeof(info));
}

bool NVDAALGsp::setRegistry(const char *key, uint32_t value) {
    // Registry entries configure GSP behavior
    struct {
        char key[64];
        uint32_t value;
    } regEntry;

    memset(&regEntry, 0, sizeof(regEntry));
    strncpy(regEntry.key, key, 63);
    regEntry.value = value;

    IOLog("NVDAAL-GSP: Setting registry %s = %u\n", key, value);

    return sendRpc(NV_VGPU_MSG_FUNCTION_SET_REGISTRY, &regEntry, sizeof(regEntry));
}
