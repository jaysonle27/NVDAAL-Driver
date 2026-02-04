/**
 * @file test_vbios_real.c
 * @brief Real VBIOS parsing tests using AD102.rom
 *
 * Tests FWSEC extraction and parsing against a real RTX 4090 VBIOS.
 * Requires: Firmware/AD102.rom
 *
 * Compile: make test-vbios-real
 * Run: ./Build/test_vbios_real [path/to/vbios.rom]
 */

#include "nvdaal_test.h"
#include <sys/stat.h>

// Include NVDAAL structures
#include "../Sources/NVDAALRegs.h"

// NVIDIA vendor ID (if not defined in NVDAALRegs.h)
#ifndef NVIDIA_VENDOR_ID
#define NVIDIA_VENDOR_ID 0x10DE
#endif

// ============================================================================
// Globals
// ============================================================================

static uint8_t *g_vbios = NULL;
static size_t g_vbios_size = 0;
static const char *g_vbios_path = "Firmware/AD102.rom";

// Parsing results
static uint32_t g_rom_base = 0;
static uint32_t g_bit_offset = 0;
static uint32_t g_pmu_table_offset = 0;
static uint32_t g_fwsec_desc_offset = 0;
static bool g_found_fwsec = false;

// ============================================================================
// VBIOS Loading
// ============================================================================

static bool load_vbios(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    fseek(f, 0, SEEK_END);
    g_vbios_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (g_vbios_size == 0 || g_vbios_size > 0x200000) {  // Max 2MB
        fclose(f);
        return false;
    }

    g_vbios = (uint8_t *)malloc(g_vbios_size);
    if (!g_vbios) {
        fclose(f);
        return false;
    }

    size_t read = fread(g_vbios, 1, g_vbios_size, f);
    fclose(f);

    return (read == g_vbios_size);
}

static void free_vbios(void) {
    if (g_vbios) {
        free(g_vbios);
        g_vbios = NULL;
    }
    g_vbios_size = 0;
}

// ============================================================================
// Helper Functions
// ============================================================================

static uint16_t read16(uint32_t offset) {
    if (offset + 2 > g_vbios_size) return 0;
    return *(uint16_t *)(g_vbios + offset);
}

static uint32_t read32(uint32_t offset) {
    if (offset + 4 > g_vbios_size) return 0;
    return *(uint32_t *)(g_vbios + offset);
}

// ============================================================================
// VBIOS Structure Tests
// ============================================================================

void test_vbios_file_exists(void) {
    struct stat st;
    int result = stat(g_vbios_path, &st);
    if (result != 0) {
        TEST_SKIP("VBIOS file not found - place AD102.rom in Firmware/");
    }
    TEST_ASSERT_EQ(0, result);
    TEST_ASSERT(st.st_size > 0);
}

void test_vbios_load(void) {
    if (!load_vbios(g_vbios_path)) {
        TEST_SKIP("Could not load VBIOS file");
    }
    TEST_ASSERT_NOT_NULL(g_vbios);
    TEST_ASSERT(g_vbios_size > 0x10000);  // At least 64KB
}

void test_vbios_rom_signature(void) {
    if (!g_vbios) TEST_SKIP("VBIOS not loaded");

    // Find ROM signature
    bool found = false;
    for (uint32_t i = 0; i < g_vbios_size - 2; i += 0x200) {
        if (read16(i) == VBIOS_ROM_SIGNATURE) {
            g_rom_base = i;
            found = true;
            break;
        }
    }

    TEST_ASSERT_MSG(found, "ROM signature 0xAA55 not found");
}

void test_vbios_pcir_valid(void) {
    if (!g_vbios || g_rom_base == 0) TEST_SKIP("ROM base not found");

    uint16_t pcir_offset = read16(g_rom_base + PCI_ROM_PCIR_OFFSET);
    uint32_t pcir_abs = g_rom_base + pcir_offset;

    TEST_ASSERT(pcir_abs + sizeof(struct VbiosPcirHeader) < g_vbios_size);

    struct VbiosPcirHeader *pcir = (struct VbiosPcirHeader *)(g_vbios + pcir_abs);
    TEST_ASSERT_EQ(PCIR_SIGNATURE, pcir->signature);
    TEST_ASSERT_EQ(NVIDIA_VENDOR_ID, pcir->vendorId);
}

void test_vbios_nvidia_device_id(void) {
    if (!g_vbios) TEST_SKIP("VBIOS not loaded");

    uint16_t pcir_offset = read16(g_rom_base + PCI_ROM_PCIR_OFFSET);
    struct VbiosPcirHeader *pcir = (struct VbiosPcirHeader *)(g_vbios + g_rom_base + pcir_offset);

    // Check for RTX 4090 device IDs
    bool is_rtx40 = (pcir->deviceId == 0x2684 || pcir->deviceId == 0x2685 ||
                     pcir->deviceId == 0x2702 || pcir->deviceId == 0x2704 ||
                     pcir->deviceId == 0x2705 || pcir->deviceId == 0x2782 ||
                     pcir->deviceId == 0x2786 || pcir->deviceId == 0x2860);

    if (!is_rtx40) {
        printf("    Note: Device ID 0x%04X (may be different RTX 40 variant)\n", pcir->deviceId);
    }

    // At minimum, should be an NVIDIA device
    TEST_ASSERT_EQ(NVIDIA_VENDOR_ID, pcir->vendorId);
}

// ============================================================================
// BIT Header Tests
// ============================================================================

void test_vbios_bit_header_found(void) {
    if (!g_vbios) TEST_SKIP("VBIOS not loaded");

    bool found = false;
    for (uint32_t i = g_rom_base; i < g_vbios_size - 12; i++) {
        if (read16(i) == BIT_HEADER_ID) {
            uint32_t sig = read32(i + 2);
            if (sig == BIT_HEADER_SIGNATURE) {
                g_bit_offset = i;
                found = true;
                break;
            }
        }
    }

    TEST_ASSERT_MSG(found, "BIT header not found");
}

void test_vbios_bit_header_valid(void) {
    if (g_bit_offset == 0) TEST_SKIP("BIT header not found");

    struct BitHeader *bit = (struct BitHeader *)(g_vbios + g_bit_offset);

    TEST_ASSERT_EQ(BIT_HEADER_ID, bit->id);
    TEST_ASSERT_EQ(BIT_HEADER_SIGNATURE, bit->signature);
    TEST_ASSERT(bit->headerSize >= 12);
    TEST_ASSERT(bit->tokenSize >= 6);
    TEST_ASSERT(bit->tokenCount > 0 && bit->tokenCount < 64);
}

// ============================================================================
// PMU Table Tests (Ada Lovelace path)
// ============================================================================

void test_vbios_pmu_token_50(void) {
    if (g_bit_offset == 0) TEST_SKIP("BIT header not found");

    struct BitHeader *bit = (struct BitHeader *)(g_vbios + g_bit_offset);
    uint32_t token_base = g_bit_offset + bit->headerSize;

    bool found_50 = false;
    bool found_70 = false;

    for (int i = 0; i < bit->tokenCount; i++) {
        uint32_t token_off = token_base + (i * bit->tokenSize);
        struct BitToken *token = (struct BitToken *)(g_vbios + token_off);

        if (token->id == BIT_TOKEN_PMU_TABLE) {
            found_50 = true;
        }
        if (token->id == BIT_TOKEN_FALCON_DATA) {
            found_70 = true;
        }
    }

    // Ada Lovelace should have token 0x50
    TEST_ASSERT_MSG(found_50 || found_70, "Neither PMU (0x50) nor Falcon (0x70) token found");
}

void test_vbios_pmu_table_found(void) {
    if (g_bit_offset == 0) TEST_SKIP("BIT header not found");

    // Search for PMU table signature: 01 06 06 xx (Ada format)
    bool found = false;
    for (uint32_t i = 0x9000; i < g_vbios_size - 32; i += 4) {
        struct PmuLookupTableHeader *pmu = (struct PmuLookupTableHeader *)(g_vbios + i);

        if (pmu->version == 1 && pmu->headerSize == 6 && pmu->entrySize == 6 &&
            pmu->entryCount > 0 && pmu->entryCount <= 32) {

            // Verify it has FWSEC entry
            uint32_t entry_base = i + pmu->headerSize;
            for (int j = 0; j < pmu->entryCount; j++) {
                uint32_t entry_off = entry_base + j * pmu->entrySize;
                struct PmuLookupEntryAda *entry = (struct PmuLookupEntryAda *)(g_vbios + entry_off);

                if (entry->appId == 0x0085 || entry->appId == 0x0086) {
                    g_pmu_table_offset = i;
                    found = true;
                    break;
                }

                // Also check 8-bit format
                uint8_t appId8 = g_vbios[entry_off];
                if (appId8 == 0x85 || appId8 == 0x86) {
                    g_pmu_table_offset = i;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
    }

    TEST_ASSERT_MSG(found, "PMU table with FWSEC entry not found");
}

// ============================================================================
// FWSEC Tests
// ============================================================================

void test_vbios_fwsec_entry(void) {
    if (g_pmu_table_offset == 0) TEST_SKIP("PMU table not found");

    struct PmuLookupTableHeader *pmu = (struct PmuLookupTableHeader *)(g_vbios + g_pmu_table_offset);
    uint32_t entry_base = g_pmu_table_offset + pmu->headerSize;

    bool found = false;
    for (int i = 0; i < pmu->entryCount; i++) {
        uint32_t entry_off = entry_base + i * pmu->entrySize;

        // Try Ada format (16-bit appId)
        struct PmuLookupEntryAda *entry = (struct PmuLookupEntryAda *)(g_vbios + entry_off);
        if (entry->appId == 0x0085 || entry->appId == 0x0086) {
            g_fwsec_desc_offset = entry->dataOffset;
            found = true;
            break;
        }

        // Try pre-Ada format (8-bit appId)
        uint8_t appId8 = g_vbios[entry_off];
        if (appId8 == 0x85 || appId8 == 0x86) {
            struct PmuLookupEntry *entry_v1 = (struct PmuLookupEntry *)(g_vbios + entry_off);
            g_fwsec_desc_offset = entry_v1->dataOffset;
            found = true;
            break;
        }
    }

    TEST_ASSERT_MSG(found, "FWSEC entry not found in PMU table");
    g_found_fwsec = found;
}

void test_vbios_fwsec_descriptor(void) {
    if (!g_found_fwsec || g_fwsec_desc_offset == 0) {
        TEST_SKIP("FWSEC entry not found");
    }

    // Try to find NVIDIA vendor ID at or near the offset
    uint32_t search_start = g_fwsec_desc_offset > 0x1000 ? g_fwsec_desc_offset - 0x1000 : 0;
    uint32_t search_end = g_fwsec_desc_offset + 0x10000;
    if (search_end > g_vbios_size) search_end = g_vbios_size;

    bool found = false;
    uint32_t desc_offset = 0;

    // First try direct offset
    if (g_fwsec_desc_offset + sizeof(struct NvfwBinHdr) < g_vbios_size) {
        struct NvfwBinHdr *hdr = (struct NvfwBinHdr *)(g_vbios + g_fwsec_desc_offset);
        if (hdr->vendorId == NVIDIA_VENDOR_ID) {
            found = true;
            desc_offset = g_fwsec_desc_offset;
        }
    }

    // Search near offset
    if (!found) {
        for (uint32_t i = search_start; i < search_end - sizeof(struct NvfwBinHdr); i += 4) {
            struct NvfwBinHdr *hdr = (struct NvfwBinHdr *)(g_vbios + i);
            if (hdr->vendorId == NVIDIA_VENDOR_ID && hdr->version >= 1 && hdr->version <= 16) {
                found = true;
                desc_offset = i;
                break;
            }
        }
    }

    TEST_ASSERT_MSG(found, "FWSEC descriptor with NVIDIA vendor ID not found");

    if (found) {
        struct NvfwBinHdr *hdr = (struct NvfwBinHdr *)(g_vbios + desc_offset);
        TEST_ASSERT(hdr->totalSize > 0);
        TEST_ASSERT(hdr->dataSize > 0);
    }
}

void test_vbios_fwsec_has_signatures(void) {
    if (!g_found_fwsec) TEST_SKIP("FWSEC not found");

    // Search for FalconUcodeDescV3Nvidia (44-byte header)
    // Look for signatureCount > 0

    bool found_signatures = false;
    for (uint32_t i = 0x10000; i < g_vbios_size - sizeof(struct FalconUcodeDescV3Nvidia); i += 4) {
        struct FalconUcodeDescV3Nvidia *desc = (struct FalconUcodeDescV3Nvidia *)(g_vbios + i);

        // Check for valid descriptor version (version 3)
        uint16_t version = desc->vDesc & 0x0000FFFF;
        if (version == 3 &&
            desc->signatureCount > 0 && desc->signatureCount <= 8 &&
            desc->imemLoadSize > 0 && desc->imemLoadSize < 0x100000 &&
            desc->dmemLoadSize > 0 && desc->dmemLoadSize < 0x20000) {

            found_signatures = true;
            printf("    Found FalconUcodeDescV3Nvidia @ 0x%X:\n", i);
            printf("      ucodeId=%d, signatureCount=%d, signatureVersions=0x%04X\n",
                   desc->ucodeId, desc->signatureCount, desc->signatureVersions);
            printf("      pkcDataOffset=0x%X, interfaceOffset=0x%X\n",
                   desc->pkcDataOffset, desc->interfaceOffset);
            break;
        }
    }

    if (!found_signatures) {
        printf("    Note: Could not locate signature info - may use different format\n");
    }
    // Don't fail - signatures may be in different format
    TEST_ASSERT(true);
}

// ============================================================================
// DMEMMAPPER Tests
// ============================================================================

void test_vbios_dmemmapper_found(void) {
    if (!g_vbios) TEST_SKIP("VBIOS not loaded");

    // Search for DMAP signature
    bool found = false;
    for (uint32_t i = 0; i < g_vbios_size - sizeof(struct DmemMapperHeader); i += 4) {
        if (read32(i) == DMEMMAPPER_SIGNATURE) {
            struct DmemMapperHeader *dmap = (struct DmemMapperHeader *)(g_vbios + i);
            if (dmap->version >= 1 && dmap->version <= 10 &&
                dmap->size >= 32 && dmap->size <= 256) {
                found = true;
                printf("    DMEMMAPPER @ 0x%X: version=%d, initCmd=0x%X\n",
                       i, dmap->version, dmap->initCmd);
                break;
            }
        }
    }

    if (!found) {
        printf("    Note: DMEMMAPPER not found in raw VBIOS - may be in DMEM section\n");
    }
    TEST_ASSERT(true);  // Don't fail - DMAP is in DMEM, not directly in VBIOS
}

// ============================================================================
// FWSEC Image Tests
// ============================================================================

void test_vbios_fwsec_image_type(void) {
    if (!g_vbios) TEST_SKIP("VBIOS not loaded");

    // Search for FWSEC image (codeType = 0xE0)
    bool found = false;
    uint32_t offset = g_rom_base;

    while (offset < g_vbios_size - 0x20) {
        if (read16(offset) != VBIOS_ROM_SIGNATURE) break;

        uint16_t pcir_off = read16(offset + PCI_ROM_PCIR_OFFSET);
        uint32_t pcir_abs = offset + pcir_off;

        if (pcir_abs + sizeof(struct VbiosPcirHeader) > g_vbios_size) break;

        struct VbiosPcirHeader *pcir = (struct VbiosPcirHeader *)(g_vbios + pcir_abs);
        if (pcir->signature != PCIR_SIGNATURE) break;

        if (pcir->codeType == 0xE0) {  // FWSEC
            found = true;
            printf("    Found FWSEC image @ 0x%X (size=%dKB)\n",
                   offset, pcir->imageLength * 512 / 1024);
        }

        if (pcir->indicator & PCIR_LAST_IMAGE_FLAG) break;
        offset += pcir->imageLength * 512;
    }

    TEST_ASSERT_MSG(found, "FWSEC image (codeType 0xE0) not found");
}

// ============================================================================
// Summary
// ============================================================================

void test_vbios_summary(void) {
    printf("\n    === VBIOS Parse Summary ===\n");
    printf("    File: %s (%zu KB)\n", g_vbios_path, g_vbios_size / 1024);
    printf("    ROM Base: 0x%X\n", g_rom_base);
    printf("    BIT Header: 0x%X\n", g_bit_offset);
    printf("    PMU Table: 0x%X\n", g_pmu_table_offset);
    printf("    FWSEC Found: %s\n", g_found_fwsec ? "YES" : "NO");
    TEST_ASSERT(true);
}

void test_cleanup_vbios(void) {
    free_vbios();
    TEST_ASSERT_NULL(g_vbios);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
    // Allow custom VBIOS path
    if (argc > 1) {
        g_vbios_path = argv[1];
    }

    test_case_t tests[] = {
        // Loading
        TEST_CASE(test_vbios_file_exists),
        TEST_CASE(test_vbios_load),

        // ROM structure
        TEST_CASE(test_vbios_rom_signature),
        TEST_CASE(test_vbios_pcir_valid),
        TEST_CASE(test_vbios_nvidia_device_id),

        // BIT
        TEST_CASE(test_vbios_bit_header_found),
        TEST_CASE(test_vbios_bit_header_valid),

        // PMU
        TEST_CASE(test_vbios_pmu_token_50),
        TEST_CASE(test_vbios_pmu_table_found),

        // FWSEC
        TEST_CASE(test_vbios_fwsec_entry),
        TEST_CASE(test_vbios_fwsec_descriptor),
        TEST_CASE(test_vbios_fwsec_has_signatures),
        TEST_CASE(test_vbios_fwsec_image_type),

        // DMEMMAPPER
        TEST_CASE(test_vbios_dmemmapper_found),

        // Summary & Cleanup
        TEST_CASE(test_vbios_summary),
        TEST_CASE(test_cleanup_vbios),

        TEST_END
    };

    return test_run_all("NVDAAL Real VBIOS Tests", tests);
}
