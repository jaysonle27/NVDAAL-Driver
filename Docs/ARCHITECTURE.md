# Arquitetura NVDAAL - Compute Driver

## Visão Geral

Driver compute-only para RTX 4090. **Sem suporte a display**.

```
+------------------------------------------------------------------+
|                     User Space                                    |
|  +---------------------------+  +-----------------------------+   |
|  |    tinygrad / PyTorch     |  |   NVDAAL User Library       |   |
|  |    (ML frameworks)        |  |   (libNVDAAL.dylib)         |   |
|  +-------------+-------------+  +-------------+---------------+   |
|                |                              |                   |
|                +---------------+--------------+                   |
|                                |                                  |
|                    +-----------v-----------+                      |
|                    |    IOKit User Client  |                      |
|                    +----------+------------+                      |
+-------------------------------|-----------------------------------+
                                |
+-------------------------------v-----------------------------------+
|                     Kernel Space                                  |
|                                                                   |
|  +-------------------------------------------------------------+  |
|  |                    NVDAAL.kext                              |  |
|  |                                                             |  |
|  |  +------------------+  +------------------+  +------------+ |  |
|  |  |  NVDAALDevice    |  |  NVDAALMemory    |  | NVDAALQueue| |  |
|  |  |  - PCI init      |  |  - VRAM alloc    |  | - Commands | |  |
|  |  |  - MMIO map      |  |  - DMA buffers   |  | - Submit   | |  |
|  |  |  - GSP init      |  |  - Page tables   |  | - Sync     | |  |
|  |  +--------+---------+  +--------+---------+  +------+-----+ |  |
|  |           |                     |                   |       |  |
|  |           +----------+----------+-------------------+       |  |
|  |                      |                                      |  |
|  |           +----------v----------+                           |  |
|  |           |    NVDAALGsp        |                           |  |
|  |           |  - RPC messages     |                           |  |
|  |           |  - Firmware load    |                           |  |
|  |           |  - Event handling   |                           |  |
|  |           +----------+----------+                           |  |
|  +----------------------|--------------------------------------+  |
+-------------------------|------------------------------------------+
                          |
+-------------------------v------------------------------------------+
|                    Hardware                                        |
|                                                                    |
|  +--------------------------------------------------------------+  |
|  |                 RTX 4090 (AD102)                             |  |
|  |                                                              |  |
|  |  +-------------+  +-------------+  +----------------------+  |  |
|  |  | GSP (RISCV) |  | GPC x12     |  | Memory Controller    |  |  |
|  |  | - Firmware  |  | - SM x128   |  | - 24GB GDDR6X        |  |  |
|  |  | - RPC       |  | - Tensors   |  | - 384-bit bus        |  |  |
|  |  +-------------+  +-------------+  +----------------------+  |  |
|  |                                                              |  |
|  +--------------------------------------------------------------+  |
+--------------------------------------------------------------------+
```

## Componentes do Kext

### NVDAALDevice (IOService)
Componente principal. Gerencia ciclo de vida.

```cpp
class NVDAALDevice : public IOService {
    // PCI e MMIO
    IOPCIDevice *pciDevice;
    IOMemoryMap *bar0Map;      // MMIO registers
    IOMemoryMap *bar1Map;      // VRAM aperture

    // Sub-componentes
    NVDAALGsp *gsp;            // GSP controller
    NVDAALMemory *memory;      // Memory manager
    NVDAALQueue *computeQueue; // Compute submission
};
```

### NVDAALGsp
Gerencia comunicação com o GSP (GPU System Processor).

```cpp
class NVDAALGsp {
    // Firmware
    IOBufferMemoryDescriptor *gspFirmware;
    IOBufferMemoryDescriptor *bootloader;

    // Message queues (DMA)
    IOBufferMemoryDescriptor *cmdQueue;
    IOBufferMemoryDescriptor *statQueue;

    // RPC
    bool sendRpc(uint32_t function, void *params, size_t size);
    bool waitEvent(uint32_t event, uint32_t timeout);
};
```

### NVDAALMemory
Gerencia alocação de memória GPU.

```cpp
class NVDAALMemory {
    // VRAM management
    uint64_t vramBase;
    uint64_t vramSize;         // 24GB para 4090

    // Allocations
    IOBufferMemoryDescriptor *allocVram(size_t size);
    IOBufferMemoryDescriptor *allocSystemDma(size_t size);
};
```

### NVDAALQueue
Submete comandos de compute para a GPU.

```cpp
class NVDAALQueue {
    // Ring buffer
    IOBufferMemoryDescriptor *ringBuffer;
    uint32_t head, tail;

    // Submission
    void pushCommand(void *cmd, size_t size);
    void kick();  // Signal GPU
    void waitIdle();
};
```

## Registros Importantes (Compute)

### GSP Registers (0x110000)
```
NV_PGSP_FALCON_MAILBOX0    0x110040  // RPC command
NV_PGSP_FALCON_MAILBOX1    0x110044  // RPC status
NV_PGSP_QUEUE_HEAD(i)      0x110C00  // Queue heads
NV_PGSP_QUEUE_TAIL(i)      0x110C80  // Queue tails
```

### RISC-V (GSP Core)
```
NV_PRISCV_RISCV_CPUCTL     0x110388  // CPU control
NV_PRISCV_RISCV_BR_RETCODE 0x110400  // Boot return code
```

### Memory Controller
```
NV_PFB_PRI_MMU_CTRL        0x100C80  // MMU control
NV_PFB_PRI_MMU_WPR2_ADDR   0x1FA824  // WPR2 address
```

### Compute Engine (CE)
```
NV_PCE_FALCON_MAILBOX0     0x104040  // CE mailbox
NV_PCE_INTR_EN             0x104100  // Interrupts
```

## Sequência de Inicialização (Compute)

```
1. PCI Probe
   └─ Verificar Device ID (0x10DE:0x2684)

2. Map BARs
   ├─ BAR0: MMIO registers
   └─ BAR1: VRAM aperture (24GB)

3. Early Init
   ├─ Ler NV_PMC_BOOT_0 (chip ID)
   ├─ Verificar WPR2 status
   └─ Reset se necessário

4. Load Firmware
   ├─ VBIOS (FWSEC ucode)
   ├─ Bootloader (kgspBinArchiveBooterLoadUcode)
   └─ GSP firmware (gsp-570.144.bin)

5. Initialize GSP
   ├─ Alocar queues DMA
   ├─ Configurar WPR metadata
   ├─ Executar bootloader
   └─ Aguardar GSP_INIT_DONE

6. Configure Compute
   ├─ Alocar compute queue
   ├─ Configurar memory manager
   └─ Registrar serviço

7. Ready for Compute!
```

## RPC Protocol

Comunicação com GSP usa mensagens RPC:

```cpp
struct RpcMessage {
    uint32_t signature;    // 0x43505256 ("VGPU")
    uint32_t version;      // 3 << 24
    uint32_t result;
    uint32_t function;
    uint32_t length;
    uint8_t  payload[];
};

// Funções importantes para compute:
#define NV_VGPU_MSG_FUNCTION_GSP_SET_SYSTEM_INFO  0x15
#define NV_VGPU_MSG_FUNCTION_SET_REGISTRY         0x16
#define NV_VGPU_MSG_FUNCTION_GSP_RM_ALLOC         0x24
#define NV_VGPU_MSG_FUNCTION_GSP_RM_CONTROL       0x25
```

## Memory Layout

```
+------------------+ 0x0
|   MMIO (BAR0)    | 16MB - Registros
+------------------+ 0x1000000
|                  |
|   VRAM (BAR1)    | 24GB - Memória GPU
|                  |
+------------------+ 0x600000000
|   GSP Heap       | 129MB
+------------------+
|   WPR2 Region    | Write Protected
+------------------+
|   Framebuffer    | (não usado - compute only)
+------------------+
```

## Diferenças macOS vs Linux

| Aspecto | Linux | macOS |
|---------|-------|-------|
| DMA Memory | dma_alloc_coherent | IOBufferMemoryDescriptor |
| MMIO Map | ioremap | mapDeviceMemoryWithIndex |
| Interrupts | request_irq | registerInterrupt |
| User Access | /dev/nvidiaX | IOUserClient |

## Próximos Passos

1. **Criar headers GSP** - Estruturas de RPC
2. **Implementar DMA** - Alocação de buffers
3. **Firmware loader** - Carregar gsp.bin
4. **RPC engine** - Enviar/receber mensagens
5. **Compute queue** - Submissão de kernels
