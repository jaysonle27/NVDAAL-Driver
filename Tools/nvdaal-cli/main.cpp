/*
 * nvdaal-cli - User-space tool for NVDAAL Driver
 *
 * Usage: nvdaal-cli boot <firmware_dir>
 *        nvdaal-cli load <gsp.bin>
 */

#include <iostream>
#include <string>
#include <fstream>
#include "../../Library/libNVDAAL.h"

void print_usage(const char *prog) {
    std::cout << "Usage: " << prog << " <command> [args]\n";
    std::cout << "Commands:\n";
    std::cout << "  status                Show GPU status (WPR2, GSP, SEC2 registers)\n";
    std::cout << "  fwsec <vbios.rom>     Load VBIOS and execute FWSEC-FRTS (configures WPR2)\n";
    std::cout << "  boot <firmware_dir>   Full boot sequence with all firmwares\n";
    std::cout << "  load <firmware.bin>   Load GSP firmware only (legacy)\n";
    std::cout << "  test                  Verify VRAM allocation and Queue kicking\n";
    std::cout << "\n";
    std::cout << "Full Boot Sequence (boot command):\n";
    std::cout << "  Expects these files in <firmware_dir>:\n";
    std::cout << "    - gsp-570.144.bin (or gsp.bin) - GSP-RM firmware\n";
    std::cout << "    - booter_load-ad102-570.144.bin (optional) - SEC2 booter\n";
    std::cout << "    - AD102.rom (optional) - VBIOS for FWSEC\n";
}

bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

int cmd_fwsec(const std::string& vbiosPath) {
    nvdaal::Client client;

    if (!client.connect()) {
        std::cerr << "[-] Error: Could not connect to driver." << std::endl;
        return 1;
    }

    std::cout << "[*] NVDAAL FWSEC Execution" << std::endl;
    std::cout << "[*] VBIOS: " << vbiosPath << std::endl;

    // Step 1: Load VBIOS
    std::cout << "[1] Loading VBIOS..." << std::endl;
    if (!client.loadVbios(vbiosPath)) {
        std::cerr << "[-] Error: Failed to load VBIOS" << std::endl;
        return 1;
    }
    std::cout << "    OK: VBIOS loaded" << std::endl;

    // Step 2: Execute FWSEC
    std::cout << "[2] Executing FWSEC-FRTS..." << std::endl;
    if (!client.executeFwsec()) {
        std::cerr << "[-] Error: FWSEC execution failed" << std::endl;
        std::cerr << "    Check: log show --predicate 'eventMessage CONTAINS \"NVDAAL\"' --last 1m" << std::endl;
        return 1;
    }
    std::cout << "    OK: FWSEC executed" << std::endl;

    // Step 3: Check WPR2
    std::cout << "[3] Checking WPR2 status..." << std::endl;
    nvdaal::GpuStatus status;
    if (client.getStatus(&status)) {
        if (status.wpr2Enabled) {
            std::cout << "[+] SUCCESS: WPR2 is now configured!" << std::endl;
            std::cout << "    WPR2_LO: 0x" << std::hex << status.wpr2Lo << std::endl;
            std::cout << "    WPR2_HI: 0x" << status.wpr2Hi << std::dec << std::endl;
            return 0;
        } else {
            std::cerr << "[-] WPR2 still not configured after FWSEC" << std::endl;
            return 1;
        }
    }

    return 1;
}

int cmd_status() {
    nvdaal::Client client;

    if (!client.connect()) {
        std::cerr << "[-] Error: Could not connect to driver." << std::endl;
        return 1;
    }

    nvdaal::GpuStatus status;
    if (!client.getStatus(&status)) {
        std::cerr << "[-] Error: Could not get GPU status." << std::endl;
        return 1;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "         NVDAAL GPU Status" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::hex << std::uppercase;

    // Chip Info
    uint32_t arch = (status.pmcBoot0 >> 20) & 0x1F;
    std::cout << "\n[Chip Info]" << std::endl;
    std::cout << "  PMC_BOOT_0:       0x" << status.pmcBoot0 << std::endl;
    std::cout << "  Architecture:     0x" << arch;
    if (arch == 0x19) std::cout << " (Ada Lovelace)";
    else if (arch == 0x17) std::cout << " (Ampere)";
    std::cout << std::endl;

    // WPR2 Status (CRITICAL for GSP boot)
    std::cout << "\n[WPR2 Status]" << std::endl;
    std::cout << "  WPR2_ADDR_LO:     0x" << status.wpr2Lo << std::endl;
    std::cout << "  WPR2_ADDR_HI:     0x" << status.wpr2Hi << std::endl;
    std::cout << "  WPR2 Enabled:     " << (status.wpr2Enabled ? "YES" : "NO") << std::endl;

    if (status.wpr2Enabled) {
        // Calculate WPR2 region
        uint64_t wpr2Start = ((uint64_t)(status.wpr2Lo & 0xFFFFF000)) << 8;
        uint64_t wpr2End = ((uint64_t)(status.wpr2Hi & 0xFFFFFFFE)) << 8;
        std::cout << "  WPR2 Region:      0x" << wpr2Start << " - 0x" << wpr2End << std::endl;
        std::cout << "  --> WPR2 is CONFIGURED (FWSEC/EFI did its job)" << std::endl;
    } else {
        std::cout << "  --> WPR2 NOT configured (need FWSEC-FRTS or EFI)" << std::endl;
    }

    // GSP RISC-V Status
    std::cout << "\n[GSP RISC-V]" << std::endl;
    std::cout << "  CPUCTL:           0x" << status.gspRiscvCpuctl << std::endl;
    bool gspHalted = (status.gspRiscvCpuctl >> 4) & 1;
    bool gspActive = (status.gspRiscvCpuctl >> 7) & 1;
    std::cout << "  Halted:           " << (gspHalted ? "YES" : "NO") << std::endl;
    std::cout << "  Active:           " << (gspActive ? "YES" : "NO") << std::endl;

    // SEC2 RISC-V Status
    std::cout << "\n[SEC2 RISC-V]" << std::endl;
    std::cout << "  CPUCTL:           0x" << status.sec2RiscvCpuctl << std::endl;
    bool sec2Halted = (status.sec2RiscvCpuctl >> 4) & 1;
    std::cout << "  Halted:           " << (sec2Halted ? "YES" : "NO") << std::endl;

    // GSP Falcon Mailboxes
    std::cout << "\n[GSP Falcon Mailbox]" << std::endl;
    std::cout << "  MAILBOX0:         0x" << status.gspFalconMailbox0 << std::endl;
    std::cout << "  MAILBOX1:         0x" << status.gspFalconMailbox1 << std::endl;

    // Boot Scratch
    std::cout << "\n[Boot Info]" << std::endl;
    std::cout << "  SCRATCH_14:       0x" << status.bootScratch << std::endl;

    std::cout << std::dec << std::nouppercase;
    std::cout << "\n========================================" << std::endl;

    return 0;
}

int cmd_boot(const std::string& firmwareDir) {
    nvdaal::Client client;

    if (!client.connect()) {
        std::cerr << "[-] Error: Could not connect to driver." << std::endl;
        return 1;
    }

    std::cout << "[*] NVDAAL Boot Sequence" << std::endl;
    std::cout << "[*] Firmware directory: " << firmwareDir << std::endl;

    // Paths to firmware files
    std::string gspPath = firmwareDir + "/gsp-570.144.bin";
    std::string booterPath = firmwareDir + "/booter_load-ad102-570.144.bin";
    std::string bootloaderPath = firmwareDir + "/bootloader-ad102-570.144.bin";
    std::string vbiosPath = firmwareDir + "/AD102.rom";

    // Fall back to generic names
    if (!file_exists(gspPath)) {
        gspPath = firmwareDir + "/gsp.bin";
    }
    if (!file_exists(vbiosPath)) {
        vbiosPath = firmwareDir + "/vbios_asus_rog_strix_4090.rom";
    }

    // Check required GSP firmware
    if (!file_exists(gspPath)) {
        std::cerr << "[-] Error: GSP firmware not found" << std::endl;
        std::cerr << "    Expected: " << firmwareDir << "/gsp-570.144.bin or gsp.bin" << std::endl;
        return 1;
    }

    // Step 1: Load VBIOS (optional - for FWSEC)
    if (file_exists(vbiosPath)) {
        std::cout << "[1] Loading VBIOS for FWSEC..." << std::endl;
        if (client.loadVbios(vbiosPath)) {
            std::cout << "    OK: VBIOS loaded" << std::endl;
        } else {
            std::cerr << "    Warning: VBIOS load failed (continuing without FWSEC)" << std::endl;
        }
    } else {
        std::cout << "[1] VBIOS not found, skipping FWSEC (may be done by EFI)" << std::endl;
    }

    // Step 2: Load bootloader (optional - GSP bootloader)
    if (file_exists(bootloaderPath)) {
        std::cout << "[2] Loading GSP bootloader..." << std::endl;
        if (client.loadBootloader(bootloaderPath)) {
            std::cout << "    OK: bootloader loaded" << std::endl;
        } else {
            std::cerr << "    Warning: bootloader load failed" << std::endl;
        }
    } else {
        std::cout << "[2] GSP bootloader not found" << std::endl;
    }

    // Step 3: Load booter_load (optional - for SEC2)
    if (file_exists(booterPath)) {
        std::cout << "[3] Loading booter_load for SEC2..." << std::endl;
        if (client.loadBooterLoad(booterPath)) {
            std::cout << "    OK: booter_load loaded" << std::endl;
        } else {
            std::cerr << "    Warning: booter_load failed (continuing with direct boot)" << std::endl;
        }
    } else {
        std::cout << "[3] booter_load not found, using direct boot" << std::endl;
    }

    // Step 4: Load GSP firmware (required)
    std::cout << "[4] Loading GSP firmware: " << gspPath << std::endl;
    if (client.loadFirmware(gspPath)) {
        std::cout << "[+] SUCCESS: GSP firmware loaded and boot sequence initiated!" << std::endl;
        return 0;
    } else {
        std::cerr << "[-] ERROR: GSP firmware load failed. Check kernel logs." << std::endl;
        std::cerr << "    Use: log show --predicate 'eventMessage contains \"NVDAAL\"' --last 1m" << std::endl;
        return 1;
    }
}

int cmd_load_firmware(const std::string& path) {
    nvdaal::Client client;

    std::cout << "[*] Loading firmware from " << path << "..." << std::endl;

    if (client.loadFirmware(path)) {
        std::cout << "[+] SUCCESS: Firmware uploaded and GSP boot initiated!" << std::endl;
        return 0;
    } else {
        std::cerr << "[-] ERROR: Failed to load firmware. Check logs." << std::endl;
        return 1;
    }
}

int cmd_test() {
    nvdaal::Client client;

    if (!client.connect()) {
        std::cerr << "[-] Error: Could not connect to driver." << std::endl;
        return 1;
    }

    std::cout << "[*] Testing VRAM allocation (1MB)..." << std::endl;
    uint64_t vramOffset = client.allocVram(1024 * 1024);

    if (vramOffset != 0) {
        std::cout << "[+] SUCCESS: Allocated at 0x" << std::hex << vramOffset << std::endl;
    } else {
        std::cerr << "[-] FAILED: VRAM allocation returned 0" << std::endl;
    }

    std::cout << "[*] Testing Compute Queue kick..." << std::endl;
    if (client.submitCommand(0xCAFEBABE)) {
        std::cout << "[+] SUCCESS: Command submitted!" << std::endl;
    } else {
        std::cerr << "[-] FAILED: Command submission error" << std::endl;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "status") {
        return cmd_status();
    } else if (command == "fwsec") {
        if (argc < 3) {
            std::cerr << "Error: Missing VBIOS path" << std::endl;
            print_usage(argv[0]);
            return 1;
        }
        return cmd_fwsec(argv[2]);
    } else if (command == "boot") {
        if (argc < 3) {
            std::cerr << "Error: Missing firmware directory path" << std::endl;
            print_usage(argv[0]);
            return 1;
        }
        return cmd_boot(argv[2]);
    } else if (command == "load") {
        if (argc < 3) {
            std::cerr << "Error: Missing firmware path" << std::endl;
            print_usage(argv[0]);
            return 1;
        }
        return cmd_load_firmware(argv[2]);
    } else if (command == "test") {
        return cmd_test();
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        print_usage(argv[0]);
        return 1;
    }
}
