#include "../../Library/libNVDAAL.h"
#include <iostream>
#include <cassert>

int main() {
    nvdaal::Client gpu;
    if (!gpu.connect()) {
        std::cerr << "Driver not loaded. Skipping test." << std::endl;
        return 0;
    }

    std::cout << "Testing MMU Bump Allocator..." << std::endl;

    uint64_t addr1 = gpu.allocVram(0x1000);
    uint64_t addr2 = gpu.allocVram(0x1000);

    std::cout << "  Alloc 1: 0x" << std::hex << addr1 << std::endl;
    std::cout << "  Alloc 2: 0x" << std::hex << addr2 << std::endl;

    assert(addr1 != 0);
    assert(addr2 != 0);
    assert(addr1 != addr2);
    assert(addr2 >= addr1 + 0x1000);

    std::cout << "MMU Test PASSED!" << std::endl;

    std::cout << "Testing Sync primitive..." << std::endl;
    bool ok = gpu.waitSemaphore(addr1, 1);
    assert(ok);
    std::cout << "Sync Test PASSED!" << std::endl;

    return 0;
}
