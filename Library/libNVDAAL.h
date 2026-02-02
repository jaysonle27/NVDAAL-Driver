/*
 * libNVDAAL.h - User-Space SDK for NVDAAL Driver
 *
 * Provides a high-level C++ API to interact with the NVIDIA RTX 4090
 * on macOS. Handles connection, memory management, and command submission.
 */

#ifndef LIB_NVDAAL_H
#define LIB_NVDAAL_H

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace nvdaal {

class Client {
public:
    Client();
    ~Client();

    // Connection
    bool connect();
    void disconnect();
    bool isConnected() const;

    // GSP Management
    bool loadFirmware(const std::string& path);
    bool loadFirmware(const void* data, size_t size);

    // Boot sequence firmware (call before loadFirmware)
    bool loadBooterLoad(const std::string& path);  // SEC2 booter firmware
    bool loadBooterLoad(const void* data, size_t size);
    bool loadVbios(const std::string& path);       // VBIOS for FWSEC
    bool loadVbios(const void* data, size_t size);

    // Memory Management
    uint64_t allocVram(size_t size);
    bool submitCommand(uint32_t cmd);
    bool waitSemaphore(uint64_t gpuAddr, uint32_t value);
    
    // Future: Command Buffer abstraction
    // struct CommandBuffer { ... };
    // bool submit(const CommandBuffer& cb);

private:
    uint32_t connection; // io_connect_t
    bool connected;
};

} // namespace nvdaal

#endif // LIB_NVDAAL_H
