/*
 * SSDT-NVDAAL.dsl - ACPI Integration for NVDAAL Compute Driver
 *
 * This SSDT improves integration between the RTX 4090 and the NVDAAL kext:
 * 1. Changes device name from "display" to avoid IONDRVFramebuffer conflict
 * 2. Injects compute-specific properties via _DSM
 * 3. Configures power management for compute workloads
 *
 * Target: RTX 4090 (AD102) at \_SB.PC00.PEG2.PEGP
 * Board: ASUS ROG Maximus Z790 Hero
 */
DefinitionBlock ("", "SSDT", 2, "NVDAAL", "RTX4090", 0x00010000)
{
    External (_SB_.PC00.PEG2, DeviceObj)
    External (_SB_.PC00.PEG2.PEGP, DeviceObj)

    // Rename device from "display" to "compute" for macOS
    // This prevents IONDRVFramebuffer from matching
    Scope (\_SB.PC00.PEG2.PEGP)
    {
        // Override the name property
        // IOKit uses this for matching - changing it avoids IONDRVFramebuffer
        Method (_DSM, 4, NotSerialized)
        {
            // Check for NVDAAL-specific UUID
            // UUID: 4E564441-414C-0000-0000-000000000000 ("NVDAAL" in ASCII)
            If (LEqual (Arg0, ToUUID ("4e564441-414c-0000-0000-000000000000")))
            {
                // Function 0: Return supported functions bitmap
                If (LEqual (Arg2, Zero))
                {
                    Return (Buffer (One) { 0x0F })  // Functions 0-3 supported
                }

                // Function 1: Return device info
                If (LEqual (Arg2, One))
                {
                    Return (Package (0x06)
                    {
                        "device-type", "compute",
                        "gpu-architecture", "ada-lovelace",
                        "compute-class", Buffer (0x04) { 0x00, 0x00, 0x12, 0x00 }
                    })
                }

                // Function 2: Return VRAM info
                If (LEqual (Arg2, 0x02))
                {
                    Return (Package (0x04)
                    {
                        "vram-size", Buffer (0x08) { 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00 },  // 24GB
                        "vram-type", "GDDR6X"
                    })
                }

                // Function 3: Return power info
                If (LEqual (Arg2, 0x03))
                {
                    Return (Package (0x04)
                    {
                        "tdp-watts", 0x01A4,  // 420W
                        "power-limit", 0x0258  // 600W max
                    })
                }
            }

            // macOS-specific properties
            If ((_OSI ("Darwin") && LEqual (Arg0, ToUUID ("a0b5b7c6-1318-441c-b0c9-fe695eaf949b"))))
            {
                If (LEqual (Arg2, Zero))
                {
                    Return (Buffer (One) { 0x03 })
                }

                Return (Package (0x10)
                {
                    // Prevent IONDRVFramebuffer matching
                    "class-code", Buffer (0x04) { 0x00, 0x00, 0x12, 0x00 },  // Processing accelerator class

                    // Device identification
                    "model", Buffer (0x10) { "NVIDIA RTX 4090" },
                    "device_type", Buffer (0x08) { "compute" },

                    // NVDAAL-specific
                    "nvdaal-compatible", One,
                    "nvdaal-version", 0x0001,

                    // Disable graphics features
                    "disable-gfx-ports", One,
                    "AAPL,no-gfx", One,

                    // Power management
                    "pci-aspm-default", Zero  // Disable ASPM for compute workloads
                })
            }

            Return (Buffer (One) { 0x00 })
        }

        // S0 wake state for compute
        Method (_S0W, 0, NotSerialized)
        {
            Return (0x04)  // D3cold capable
        }

        // Power state methods
        Method (_PS0, 0, NotSerialized)  // D0 - Full power
        {
            // GPU enters full power state
            // Compute workloads require full power
        }

        Method (_PS3, 0, NotSerialized)  // D3 - Off
        {
            // GPU enters low power state
            // Safe for idle compute driver
        }

        // Device presence check
        Method (_STA, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                Return (0x0F)  // Present, enabled, functioning
            }
            Else
            {
                Return (0x0F)
            }
        }
    }

    // HD Audio device on RTX 4090 (HDAU)
    // Path: \_SB.PC00.PEG2.PEGP.HDAU (Address 0,1)
    Scope (\_SB.PC00.PEG2)
    {
        Device (HDAU)
        {
            Name (_ADR, One)  // Function 1

            Method (_DSM, 4, NotSerialized)
            {
                If ((_OSI ("Darwin") && LEqual (Arg0, ToUUID ("a0b5b7c6-1318-441c-b0c9-fe695eaf949b"))))
                {
                    If (LEqual (Arg2, Zero))
                    {
                        Return (Buffer (One) { 0x03 })
                    }

                    Return (Package (0x04)
                    {
                        "hda-gfx", Buffer (0x0A) { "onboard-2" },
                        "layout-id", Buffer (0x04) { 0x07, 0x00, 0x00, 0x00 }
                    })
                }

                Return (Buffer (One) { 0x00 })
            }

            Method (_STA, 0, NotSerialized)
            {
                If (_OSI ("Darwin"))
                {
                    Return (0x0F)
                }
                Else
                {
                    Return (Zero)
                }
            }
        }
    }
}
