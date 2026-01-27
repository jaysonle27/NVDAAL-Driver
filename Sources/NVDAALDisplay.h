/*
 * NVDAALDisplay.h - Fake Display Engine for Metal Detection
 *
 * Tries to spoof an IOGraphicsDevice to make macOS recognize
 * the card as a display adapter in System Information.
 */

#ifndef NVDAAL_DISPLAY_H
#define NVDAAL_DISPLAY_H

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

class NVDAALDisplay : public IOService {
    OSDeclareDefaultStructors(NVDAALDisplay);

private:
    IOPCIDevice *pciDevice;

public:
    static NVDAALDisplay* withDevice(IOPCIDevice *dev);
    
    virtual bool init() override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;

    // Inject properties to trick WindowServer
    void injectGraphcisProperties();
};

#endif // NVDAAL_DISPLAY_H
