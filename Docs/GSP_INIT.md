# GSP (GPU System Processor) Initialization

Based on TinyGPU (tinygrad) analysis - NVIDIA driver for macOS.

## Overview

The GSP is a RISC-V processor inside the NVIDIA GPU that manages:
- Hardware initialization
- Power management
- Driver communication

**IMPORTANT**: For RTX 40 series (Ada Lovelace), the GSP is MANDATORY.

## Required Firmware

| File | Source | Description |
|------|--------|-------------|
| `gsp-570.144.bin` | linux-firmware/nvidia | Main GSP firmware (~30MB) |
| `kgspBinArchiveBooterLoadUcode` | open-gpu-kernel-modules | Bootloader |
| `kgspBinArchiveGspRmBoot` | open-gpu-kernel-modules | Boot binary |
| VBIOS | TechPowerUp / GPU | Card BIOS |

### Firmware Download

```bash
# GSP Firmware (from NVIDIA linux-firmware)
curl -L -o gsp-570.144.bin \
  "https://github.com/NVIDIA/linux-firmware/raw/refs/heads/nvidia-staging/nvidia/ga102/gsp/gsp-570.144.bin"

# Headers and code (from open-gpu-kernel-modules)
git clone https://github.com/NVIDIA/open-gpu-kernel-modules
```

## Initialization Sequence

### 1. Early Init
```
1. Read chip ID (NV_PMC_BOOT_0, NV_PMC_BOOT_42)
2. Detect architecture (GA1=Ampere, AD1=Ada, GB2=Blackwell)
3. Check if WPR2 is active (if yes, PCI reset)
```

### 2. Prepare VBIOS/FWSEC
```
1. Read VBIOS from GPU (offset 0x300000 in MMIO)
2. Parse BIT header and tables
3. Extract FWSEC ucode from VBIOS
4. Configure FRTS (Firmware Runtime Scratch) region
```

### 3. Prepare Booter
```
1. Extract kgspBinArchiveBooterLoadUcode
2. Apply signature patch
3. Allocate in sysmem (DMA-able)
```

### 4. Prepare GSP Image
```
1. Load gsp-570.144.bin (ELF)
2. Extract .fwimage section
3. Extract signature (.fwsignature_ad10x)
4. Build radix3 page table:
   - Level 0: 1 page
   - Level 1: N pages (indices to level 2)
   - Level 2: N pages (indices to level 3)
   - Level 3: Firmware pages
```

### 5. Configure WPR Meta
```c
GspFwWprMeta {
    sizeOfBootloader
    sysmemAddrOfBootloader
    sizeOfRadix3Elf
    sysmemAddrOfRadix3Elf
    sizeOfSignature
    sysmemAddrOfSignature
    bootloaderCodeOffset
    bootloaderDataOffset
    gspFwHeapSize = 0x8100000  // ~129MB
    frtsSize = 0x100000        // 1MB
    // ... more fields
}
```

### 6. Send Initial RPCs
```
1. NV_VGPU_MSG_FUNCTION_GSP_SET_SYSTEM_INFO
   - Physical addresses of BARs
   - PCI device/vendor IDs

2. NV_VGPU_MSG_FUNCTION_SET_REGISTRY
   - RMForcePcieConfigSave = 1
   - RMSecBusResetEnable = 1
```

### 7. Execute Falcon/Booter
```
For Ampere/Ada (no FMC boot):
1. Reset FALCON
2. Execute FWSEC (frts_image)
3. Verify WPR2 initialized
4. Reset FALCON (RISC-V mode)
5. Configure mailbox with libos_args
6. Reset SEC2
7. Execute booter
8. Verify handoff
```

### 8. Wait for GSP_INIT_DONE
```python
stat_q.wait_resp(NV_VGPU_MSG_EVENT_GSP_INIT_DONE)
```

## Important Registers

### FALCON (0x00110000)
```
NV_PFALCON_FALCON_MAILBOX0/1  - GSP communication
NV_PFALCON_FALCON_DMATRFCMD   - DMA control
NV_PFALCON_FALCON_CPUCTL      - CPU control
NV_PFALCON_FALCON_BOOTVEC     - Boot vector
```

### MMU/WPR
```
NV_PFB_PRI_MMU_WPR2_ADDR_HI   - WPR2 address (Write Protected Region)
```

### GSP
```
NV_PGSP_FALCON_MAILBOX0/1     - GSP mailbox
NV_PGSP_QUEUE_HEAD            - Command queue head
```

### RISC-V
```
NV_PRISCV_RISCV_CPUCTL        - RISC-V control
NV_PRISCV_RISCV_BCR_CTRL      - Boot config
```

## RPC Structure

```c
// RPC message header
struct rpc_message_header_v {
    uint32_t signature;      // NV_VGPU_MSG_SIGNATURE_VALID
    uint32_t header_version; // (3 << 24)
    uint32_t rpc_result;
    uint32_t function;       // Function code
    uint32_t length;         // Total size
};

// Queue element
struct GSP_MSG_QUEUE_ELEMENT {
    uint32_t checkSum;
    uint32_t elemCount;      // How many 4KB messages
    uint32_t seqNum;
    // ... message data
};
```

## Main RPC Functions

| Function | Code | Description |
|----------|------|-------------|
| GSP_SET_SYSTEM_INFO | 0x15 | Configure system info |
| SET_REGISTRY | 0x16 | Configure registry |
| GSP_RM_ALLOC | 0x24 | Allocate RM objects |
| GSP_RM_CONTROL | 0x25 | Control RM objects |
| GSP_INIT_DONE | 0x52 | Event: GSP ready |

## For NVDAAL

### Next Steps:
1. Port data structures (nv.h) to IOKit
2. Implement DMA-able sysmem allocation
3. Implement RPC communication via MMIO
4. Download and integrate GSP firmware
5. Implement boot sequence

### macOS vs Linux Differences:
- IOKit instead of /dev/nv*
- IOBufferMemoryDescriptor for DMA
- No direct mmap (use IOMemoryMap)

## References

- [TinyGPU/tinygrad](https://github.com/tinygrad/tinygrad)
- [NVIDIA open-gpu-kernel-modules](https://github.com/NVIDIA/open-gpu-kernel-modules)
- [NVIDIA linux-firmware](https://github.com/NVIDIA/linux-firmware)
- [Nouveau/Mesa](https://nouveau.freedesktop.org/)
