# Reverse Engineering Report - RTX 4090 (AD102)

> Generated from live RTX 4090 via WSL2 on Windows 11 (NVIDIA 591.74)
> Date: 2026-02-06

## Hardware Profile

| Property | Value |
|----------|-------|
| GPU | NVIDIA GeForce RTX 4090 |
| Chip | AD102-300-A1 |
| PCI Device ID | 0x268410DE |
| PCI SubSystem ID | 0x889D1043 (ASUS ROG) |
| VBIOS | 95.02.18.80.87 |
| Architecture | Ada Lovelace (0x19) |
| Compute Capability | 8.9 (SM89) |
| SMs | 128 |
| VRAM | 24.0 GB (0x600000000) |
| VRAM Usable | 23.99 GB (0x5FF380000) |
| FW Reserved | 12.5 MB |
| BAR1 | 256 MB (Windows WDDM) |
| PCIe | Gen4 x16 (running x8 on test system) |
| Max GPU Clock | 3105 MHz |
| Max Memory Clock | 10501 MHz |
| Power Limit | 477 W |
| CUDA Version | 12.8 / 13.1 |

## Compute Benchmarks (Reference)

| Test | Time | Performance |
|------|------|-------------|
| FP32 Matmul 1024x1024 | 4.5 ms | 0.5 TFLOPS |
| FP32 Matmul 2048x2048 | 0.5 ms | 35.5 TFLOPS |
| FP32 Matmul 4096x4096 | 2.7 ms | 51.2 TFLOPS |
| FP32 Matmul 8192x8192 | 20.0 ms | 55.0 TFLOPS |
| FP16 Tensor Cores 8192 | 19.7 ms | 55.9 TFLOPS |
| Memory Bandwidth | - | 304 GB/s |

This is the target performance NVDAAL should achieve on macOS.

## Critical Finding: FWSEC Not in VBIOS

**On Ada Lovelace, the VBIOS ROM does NOT contain FWSEC firmware.**

Confirmed by scanning two different VBIOS images:
1. Live GPU VBIOS (read via BAR0 @ 0x300000): 2 MB
2. ASUS ROG Strix 4090 VBIOS dump: 2 MB

Both contain only:
- Image 1: x86 BIOS (codeType 0x00) - 64 KB
- Image 2: EFI driver (codeType 0x03) - 85 KB

**No codeType 0xE0 (FWSEC/DEVINIT) image exists.**

### Where FWSEC Lives

On Ada Lovelace, FWSEC is provided as a separate firmware file:
- **Linux**: `nvidia/ad102/gsp/fwsec-frts-ad10x.bin` (from linux-firmware repo)
- **Windows**: Embedded in `gsp_ga10x.bin` (69.8 MB ELF RISC-V) or `nvlddmkm.sys` (100 MB)

### Impact on NVDAAL

- **Method 3B (VBIOS parse for FWSEC)**: Will NEVER work on Ada. Remove or skip.
- **Method 3A (file-based FWSEC)**: Correct approach, but needs the real `fwsec-frts-ad10x.bin`
- The current `fwsec.bin` (66KB FWSC format) was extracted via `extract_fwsec.py` but has invalid RSA signatures.

## WPR2 (Write Protected Region 2)

### Expected Values

| Register | Offset | Expected Value |
|----------|--------|---------------|
| WPR2_ADDR_LO | 0x001FA824 | 0x05FFF000 |
| WPR2_ADDR_HI | 0x001FA828 | 0x06000000 |

FRTS goes at the top 1 MB of VRAM:
- FRTS Offset: 0x5FFF00000 (24 GB - 1 MB)
- FRTS Size: 0x100000 (1 MB)
- FRTS End: 0x600000000 (24 GB)

### Register Offset Investigation

The NVIDIA Windows driver (`nvlddmkm.sys`) references:
- `0x1FA824` as WPR2_ADDR_LO (3 references)
- `0x1FA828` as WPR2_ADDR_HI (4 references)

**Potential issue**: The kext currently defines:
```c
#define NV_PFB_PRI_MMU_WPR2_ADDR_LO  0x001FA820  // ← Might be wrong for Ada
#define NV_PFB_PRI_MMU_WPR2_ADDR_HI  0x001FA824  // ← Might be wrong for Ada
```

If the registers shifted by 4 bytes on Ada, this would explain why:
- Reading 0x1FA820 returns 0 (wrong register)
- Reading 0x1FA824 returns 0x1FFFFE00 (actually WPR2_LO, not HI)

**TODO**: Verify correct register offsets for AD102 (Ada Lovelace).

## NVIDIA Windows Driver Analysis

### Driver Structure (2.54 GB total)

| File | Size | Purpose |
|------|------|---------|
| nvlddmkm.sys | 100 MB | Kernel-mode display driver |
| gsp_ga10x.bin | 70 MB | GSP firmware (Ada/Ampere - ELF RISC-V) |
| gsp_tu10x.bin | 29 MB | GSP firmware (Turing) |
| nvcubins.bin | 78 MB | CUDA binaries |
| nvcoproc.bin | 12 MB | Coprocessor firmware |
| nvoptix.bin | 46 MB | OptiX raytracing |

### GSP Firmware (gsp_ga10x.bin)

ELF format (RISC-V 64-bit, Little Endian, Relocatable):
- `.fwimage`: 69.8 MB (main firmware image)
- `.fwversion`: `590.52.01`
- `.fwsignature_ad10x`: 4096 bytes (Ada Lovelace RSA signature)
- `.fwsignature_ga10x`: 4096 bytes (Ampere signature)
- `.fwsignature_gb10x/gb20x/gh100`: Blackwell/Hopper signatures

The firmware contains 128 references to "FWSEC" as error/status strings and 3 "FRTS" references, confirming FWSEC logic is embedded in the GSP firmware itself.

### Key Functions (from nvlddmkm.sys strings)

| Function | Purpose |
|----------|---------|
| `RMExecuteFwsecOnSec2` | **FWSEC executes on SEC2 Falcon, NOT GSP** |
| `RmDisableFwseclic` | Registry key to disable FWSEC |
| `RMDevinitBySecureBoot` | DEVINIT via secure boot |
| `RMExecuteDevinitOnPmu` | DEVINIT on PMU |
| `RmDisableSec2Load` | Disable SEC2 loading |
| `RmSec2EnableRtos` | SEC2 RTOS mode |
| `acrGatherWprInformation_GM200` | WPR info gathering |
| `lsfmFalconReset_IMPL` | Falcon reset implementation |

### Key Insight: FWSEC runs on SEC2

The string `RMExecuteFwsecOnSec2` confirms that FWSEC-FRTS should be loaded and executed on the **SEC2 Falcon** (base 0x840000), not the GSP Falcon (base 0x110000).

The current NVDAAL EFI driver loads FWSEC on the GSP Falcon. This may be incorrect.

### Register References in nvlddmkm.sys

| Register | Offset | References |
|----------|--------|-----------|
| GSP_RISCV_BASE | 0x00118000 | 68 |
| SEC2_RISCV_BASE | 0x00841000 | 49 |
| WPR2_ADDR_LO | 0x001FA824 | 3 |
| WPR2_ADDR_HI | 0x001FA828 | 4 |

## VBIOS Structure

### ROM Images

| Offset | Type | Size |
|--------|------|------|
| 0x000000 | x86 BIOS (0x00) | 64 KB |
| 0x00FC00 | EFI (0x03) | 85 KB |

### BIT Table (19 tokens)

| # | ID | Name | Version | Pointer |
|---|-----|------|---------|---------|
| 0 | 0x32 | INIT | 1 | 0x023E |
| 7 | 0x50 | PERF | 2 | 0x02EC |
| 14 | 0x70 | FALCON_DATA | 2 | 0x041F |

### FALCON_DATA Analysis

- Pointer: 0x041F
- UcodeTablePtr: 0x00080DE8
- Data at 0x80DE8: Repeating pattern `E7 8F 84 00` (NOT a valid PMU table)
- **This is NOT a PMU Lookup Table** - it's likely clock/power data
- Ada Lovelace does not use this path for FWSEC

## Firmware Files Extracted

| File | Size | Source |
|------|------|--------|
| `Firmware/ad102/fwsignature_ad10x.bin` | 4096 bytes | From gsp_ga10x.bin |
| `Firmware/ad102/booter_load-570.144.bin` | 57720 bytes | From linux-firmware |
| `Firmware/ad102/booter_unload-570.144.bin` | 41592 bytes | From linux-firmware |
| `Docs/gpu_reference_rtx4090.json` | 3274 bytes | Live GPU dump |

## Recommendations

1. **Use SEC2 Falcon** (0x840000) for FWSEC execution, not GSP Falcon (0x110000)
2. **Fix WPR2 register offsets** for Ada: LO=0x1FA824, HI=0x1FA828
3. **Remove VBIOS-based FWSEC extraction** (Method 3B) - never works on Ada
4. **Obtain real fwsec-frts-ad10x.bin** from linux-firmware (not publicly hosted)
5. **Extract FWSEC from gsp_ga10x.bin** .fwimage section as alternative
6. **Verify .fwsignature_ad10x** is the correct signature format for the driver
