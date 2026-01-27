/*
 * hello_compute.cpp - Example of using NVDAAL for Compute
 *
 * This demonstrates how to allocate VRAM and submit a command.
 * In a real app, you would compile this against libNVDAAL.
 */

#include "../../Library/libNVDAAL.h"
#include <iostream>
#include <vector>

int main() {
    nvdaal::Client gpu;

    if (!gpu.connect()) {
        std::cerr << "Failed to connect to GPU driver." << std::endl;
        return 1;
    }

    std::cout << "Connected to NVIDIA Ada Lovelace!" << std::endl;

    // 1. Allocate buffers (Input A, Input B, Output C)
    // 1MB each
    size_t bufferSize = 1024 * 1024;
    
    uint64_t bufA = gpu.allocVram(bufferSize);
    uint64_t bufB = gpu.allocVram(bufferSize);
    uint64_t bufC = gpu.allocVram(bufferSize);

    if (!bufA || !bufB || !bufC) {
        std::cerr << "Failed to allocate VRAM." << std::endl;
        return 1;
    }

    std::cout << "Allocated buffers:" << std::endl;
    std::cout << "  A: 0x" << std::hex << bufA << std::endl;
    std::cout << "  B: 0x" << std::hex << bufB << std::endl;
    std::cout << "  C: 0x" << std::hex << bufC << std::endl;

    // 2. Submit Compute Kernel
    // (Here we send a placeholder, but this would be the actual shader launch)
    // In real CUDA, this is: method_launch(grid, block, params...)
    
    std::cout << "Launching compute kernel..." << std::endl;
    if (gpu.submitCommand(0x1337C0DE)) {
        std::cout << "Kernel submitted successfully!" << std::endl;
    } else {
        std::cerr << "Kernel submission failed." << std::endl;
        return 1;
    }

    // 3. Wait for completion (Sync)
    // TODO: implement sync object waiting
    
    return 0;
}