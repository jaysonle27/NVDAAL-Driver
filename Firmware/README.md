# Firmware for NVDAAL

This directory should contain firmware files extracted from the GPU.

## Required Files

| File | Description | Source |
|------|-------------|--------|
| `vbios.rom` | VBIOS extracted from GPU | `extract_vbios.py` |
| `gsp.bin` | GSP Firmware (optional) | Linux driver |
| `acr.bin` | ACR firmware (optional) | Linux driver |

## How to Extract

### VBIOS (Method 1 - Direct Linux)

```bash
cd ../Tools
sudo python3 extract_vbios.py -o ../Firmware/vbios.rom
```

### VBIOS (Method 2 - TechPowerUp)

1. Go to https://www.techpowerup.com/vgabios/
2. Search for "RTX 4090"
3. Download the version compatible with your card
4. Save as `vbios.rom` in this directory

### GSP Firmware (from Linux driver)

```bash
# On Linux with NVIDIA driver installed
cp /lib/firmware/nvidia/gsp_ga10x.bin ./gsp.bin
```

## OpenCore Injection

To inject VBIOS via OpenCore:

1. Convert to Base64:
```bash
base64 -i vbios.rom -o vbios.txt
```

2. Add to config.plist:
```xml
<key>DeviceProperties</key>
<dict>
    <key>Add</key>
    <dict>
        <key>PciRoot(0x0)/Pci(0x1,0x0)/Pci(0x0,0x0)</key>
        <dict>
            <key>rom</key>
            <data><!-- Paste vbios.txt content here --></data>
        </dict>
    </dict>
</dict>
```

## Legal Notice

Using proprietary firmware may violate NVIDIA's license terms.
This project is for educational and research purposes only.
