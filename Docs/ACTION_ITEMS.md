# ACTION ITEMS - NVDAAL Driver Fixes

> Based on reverse engineering session 2026-02-06
> RTX 4090 (AD102) with NVIDIA driver 591.74 loaded

## Critical Bug #1: WPR2 Register Offsets (CONFIRMED WRONG)

**Files**: `NVDAALRegs.h`, `NvdaalFwsec.c`

Current (WRONG for Ada Lovelace):
```c
#define NV_PFB_PRI_MMU_WPR2_ADDR_LO  0x001FA820
#define NV_PFB_PRI_MMU_WPR2_ADDR_HI  0x001FA824
```

Correct (confirmed via NVIDIA driver 591.74):
```c
#define NV_PFB_PRI_MMU_WPR2_ADDR_LO  0x001FA824
#define NV_PFB_PRI_MMU_WPR2_ADDR_HI  0x001FA828
```

**Evidence**: nvlddmkm.sys has 3 refs to 0x1FA824 (LO) and 4 refs to 0x1FA828 (HI).
Zero references to 0x1FA820.

**Impact**: This is why WPR2 reads return wrong values. Reading 0x1FA820 returns garbage,
reading 0x1FA824 returns what looks like HI but is actually LO.

## Critical Bug #2: FWSEC Execution Target (CONFIRMED WRONG)

**Files**: `NVDAALGsp.cpp` (line 1117, 1963), `NvdaalFwsec.c`

Current (WRONG):
```c
const uint32_t falconBase = NV_PGSP_BASE;  // 0x110000 (GSP Falcon)
```

Correct:
```c
const uint32_t falconBase = NV_PSEC_BASE;  // 0x840000 (SEC2 Falcon)
```

**Evidence**: `RMExecuteFwsecOnSec2` string in nvlddmkm.sys.
SEC2 FALCON (0x840000) has 49+ references in the driver.

## Critical Bug #3: VBIOS FWSEC Parsing (WILL NEVER WORK ON ADA)

**Files**: `NVDAALGsp.cpp` (parseVbios, executeFwsecFrts)

Ada Lovelace VBIOS contains only:
- Image 0: x86 BIOS (codeType 0x00) - 64 KB
- Image 1: EFI driver (codeType 0x03) - 85 KB
- **NO codeType 0xE0 (FWSEC) image**

On Ada, FWSEC comes from linux-firmware as a separate file:
`nvidia/ad102/gsp/fwsec-frts-ad10x.bin`

**Action**: Add early return in parseVbios() on Ada arch to skip futile scanning.

## Data: GSP Firmware Structure (gsp_ga10x.bin)

- Size: 73,226,224 bytes (69.8 MB)
- Format: ELF 64-bit RISC-V (machine 0x00F3)
- 17 sections including 10 signature sections
- `.fwimage`: 73,183,232 bytes (main firmware)
- `.fwversion`: "590.52.01"
- `.fwsignature_ad10x`: 4096 bytes (Ada RSA signature)
- **FWSEC is NOT embedded in .fwimage** (0 FWSEC/DMEMMAPPER/FWSC markers)

## Data: Live GPU State (driver loaded)

| Property | Value |
|----------|-------|
| Driver | 591.74 |
| NVML | 13.590.52.01 |
| CUDA | 13.1 (13010) |
| VRAM Usable | 0x5FF400000 (23.99 GB) |
| VRAM Physical | 0x600000000 (24.0 GB) |
| FW Reserved | ~12 MB |
| BAR1 | 256 MB |
| VBIOS | 95.02.18.80.87 |
| PCIe | Gen4 x8 (max x16) |
| Temp | 34C idle |
| Power | 15W idle / 477W max |

## WPR2 Expected Values

```
Physical VRAM:  0x600000000 (24 GB)
FRTS Offset:    0x5FFF00000 (24 GB - 1 MB)
FRTS Size:      0x100000 (1 MB)
FRTS End:       0x600000000

WPR2_LO register: 0x05FFF0000 >> 12 = 0x5FFF0 (shifted)
WPR2_HI register: 0x600000000 >> 12 = 0x60000 (shifted)
```

## Firmware Files Status

| File | Status | Size |
|------|--------|------|
| booter_load-570.144.bin | OK | 57,720 bytes |
| booter_unload-570.144.bin | OK | 41,592 bytes |
| fwsignature_ad10x.bin | OK | 4,096 bytes |
| fwsec-frts-ad10x.bin | MISSING | Not publicly available |
| fwsec-hs-ad10x.bin | MISSING | Not publicly available |

## Next Steps (Priority Order)

1. Fix WPR2 register offsets (commit 1)
2. Fix FWSEC execution target to SEC2 (commit 2)
3. Add Ada VBIOS skip in parseVbios (commit 3)
4. Update EFI driver with same fixes (commit 4)
5. Obtain fwsec-frts-ad10x.bin (manual: `apt install linux-firmware` on Linux)
6. Test on hackintosh
