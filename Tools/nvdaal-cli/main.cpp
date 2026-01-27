/*
 * nvdaal-cli - User-space tool for NVDAAL Driver
 *
 * Usage: nvdaal-cli load <path/to/gsp.bin>
 */

#include <iostream>
#include <string>
#include "../../Library/libNVDAAL.h"

void print_usage(const char *prog) {
    std::cout << "Usage: " << prog << " <command> [args]\n";
    std::cout << "Commands:\n";
    std::cout << "  load <firmware.bin>   Load GSP firmware into the GPU\n";
    std::cout << "  test                  Verify VRAM allocation and Queue kicking\n";
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

    if (command == "load") {
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
