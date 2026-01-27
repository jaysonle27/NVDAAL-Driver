import ctypes
import os

# Path to the shared library
lib_path = os.path.abspath("Build/libNVDAAL.dylib")

if not os.path.exists(lib_path):
    print(f"Error: {lib_path} not found. Run 'make' first.")
    exit(1)

# Load the library
nvdaal = ctypes.CDLL(lib_path)

# Define function signatures
nvdaal.nvdaal_create_client.restype = ctypes.c_void_p
nvdaal.nvdaal_destroy_client.argtypes = [ctypes.c_void_p]
nvdaal.nvdaal_connect.argtypes = [ctypes.c_void_p]
nvdaal.nvdaal_connect.restype = ctypes.c_bool
nvdaal.nvdaal_alloc_vram.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
nvdaal.nvdaal_alloc_vram.restype = ctypes.c_uint64
nvdaal.nvdaal_submit_command.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
nvdaal.nvdaal_submit_command.restype = ctypes.c_bool

print("--- NVDAAL Python Bridge Test ---")

# 1. Create Client
client = nvdaal.nvdaal_create_client()
if not client:
    print("[-] Failed to create client instance.")
    exit(1)

try:
    # 2. Connect to Driver
    print("[*] Connecting to NVDAAL driver...")
    if not nvdaal.nvdaal_connect(client):
        print("[-] Driver not found or connection failed. Is the kext loaded?")
    else:
        print("[+] Connected to NVIDIA Ada Lovelace!")

        # 3. Test VRAM Allocation
        size = 1024 * 1024 # 1MB
        print(f"[*] Requesting {size//1024}KB of VRAM...")
        addr = nvdaal.nvdaal_alloc_vram(client, size)
        if addr != 0:
            print(f"[+] VRAM allocated at physical offset: 0x{addr:016x}")
        else:
            print("[-] VRAM allocation failed.")

        # 4. Test Command Submission
        print("[*] Submitting NOP command to Queue...")
        if nvdaal.nvdaal_submit_command(client, 0x0):
            print("[+] Command submitted and GPU signaled!")
        else:
            print("[-] Command submission failed.")

finally:
    # 5. Cleanup
    nvdaal.nvdaal_destroy_client(client)
    print("[*] Client destroyed. Test finished.")
