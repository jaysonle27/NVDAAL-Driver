# Inicialização do GSP (GPU System Processor)

Baseado na análise do TinyGPU (tinygrad) - driver NVIDIA para macOS.

## Visão Geral

O GSP é um processador RISC-V dentro da GPU NVIDIA que gerencia:
- Inicialização do hardware
- Power management
- Comunicação com o driver

**IMPORTANTE**: Para RTX 40 series (Ada Lovelace), o GSP é OBRIGATÓRIO.

## Firmware Necessário

| Arquivo | Fonte | Descrição |
|---------|-------|-----------|
| `gsp-570.144.bin` | linux-firmware/nvidia | Firmware principal GSP (~30MB) |
| `kgspBinArchiveBooterLoadUcode` | open-gpu-kernel-modules | Bootloader |
| `kgspBinArchiveGspRmBoot` | open-gpu-kernel-modules | Boot binary |
| VBIOS | TechPowerUp / GPU | BIOS da placa |

### Download do Firmware

```bash
# GSP Firmware (da NVIDIA linux-firmware)
curl -L -o gsp-570.144.bin \
  "https://github.com/NVIDIA/linux-firmware/raw/refs/heads/nvidia-staging/nvidia/ga102/gsp/gsp-570.144.bin"

# Headers e código (do open-gpu-kernel-modules)
git clone https://github.com/NVIDIA/open-gpu-kernel-modules
```

## Sequência de Inicialização

### 1. Early Init
```
1. Ler chip ID (NV_PMC_BOOT_0, NV_PMC_BOOT_42)
2. Detectar arquitetura (GA1=Ampere, AD1=Ada, GB2=Blackwell)
3. Verificar se WPR2 está ativo (se sim, reset PCI)
```

### 2. Preparar VBIOS/FWSEC
```
1. Ler VBIOS da GPU (offset 0x300000 no MMIO)
2. Parsear BIT header e tabelas
3. Extrair FWSEC ucode do VBIOS
4. Configurar FRTS (Firmware Runtime Scratch) region
```

### 3. Preparar Booter
```
1. Extrair kgspBinArchiveBooterLoadUcode
2. Aplicar patch de assinatura
3. Alocar em sysmem (DMA-able)
```

### 4. Preparar GSP Image
```
1. Carregar gsp-570.144.bin (ELF)
2. Extrair seção .fwimage
3. Extrair assinatura (.fwsignature_ad10x)
4. Construir radix3 page table:
   - Level 0: 1 página
   - Level 1: N páginas (índices para level 2)
   - Level 2: N páginas (índices para level 3)
   - Level 3: Páginas do firmware
```

### 5. Configurar WPR Meta
```c
GspFwWprMeta {
    sizeOfBootloader
    sysmemAddrOfBootloader
    sizeOfRadix3Elf
    sysmemAddrOfRadix3Elf
    sizeOfSignature
    sysmemAddrOfSignature
    bootloaderCodeOffset
    bootloaderDataOffset
    gspFwHeapSize = 0x8100000  // ~129MB
    frtsSize = 0x100000        // 1MB
    // ... mais campos
}
```

### 6. Enviar RPCs Iniciais
```
1. NV_VGPU_MSG_FUNCTION_GSP_SET_SYSTEM_INFO
   - Endereços físicos das BARs
   - PCI device/vendor IDs

2. NV_VGPU_MSG_FUNCTION_SET_REGISTRY
   - RMForcePcieConfigSave = 1
   - RMSecBusResetEnable = 1
```

### 7. Executar Falcon/Booter
```
Para Ampere/Ada (sem FMC boot):
1. Reset FALCON
2. Executar FWSEC (frts_image)
3. Verificar WPR2 inicializado
4. Reset FALCON (modo RISC-V)
5. Configurar mailbox com libos_args
6. Reset SEC2
7. Executar booter
8. Verificar handoff
```

### 8. Aguardar GSP_INIT_DONE
```python
stat_q.wait_resp(NV_VGPU_MSG_EVENT_GSP_INIT_DONE)
```

## Registros Importantes

### FALCON (0x00110000)
```
NV_PFALCON_FALCON_MAILBOX0/1  - Comunicação com GSP
NV_PFALCON_FALCON_DMATRFCMD   - Controle de DMA
NV_PFALCON_FALCON_CPUCTL      - Controle da CPU
NV_PFALCON_FALCON_BOOTVEC     - Vetor de boot
```

### MMU/WPR
```
NV_PFB_PRI_MMU_WPR2_ADDR_HI   - Endereço WPR2 (Write Protected Region)
```

### GSP
```
NV_PGSP_FALCON_MAILBOX0/1     - Mailbox do GSP
NV_PGSP_QUEUE_HEAD            - Head da fila de comandos
```

### RISC-V
```
NV_PRISCV_RISCV_CPUCTL        - Controle do RISC-V
NV_PRISCV_RISCV_BCR_CTRL      - Boot config
```

## Estrutura de RPC

```c
// Header de mensagem RPC
struct rpc_message_header_v {
    uint32_t signature;      // NV_VGPU_MSG_SIGNATURE_VALID
    uint32_t header_version; // (3 << 24)
    uint32_t rpc_result;
    uint32_t function;       // Código da função
    uint32_t length;         // Tamanho total
};

// Elemento da fila
struct GSP_MSG_QUEUE_ELEMENT {
    uint32_t checkSum;
    uint32_t elemCount;      // Quantas mensagens de 4KB
    uint32_t seqNum;
    // ... dados da mensagem
};
```

## Funções RPC Principais

| Função | Código | Descrição |
|--------|--------|-----------|
| GSP_SET_SYSTEM_INFO | 0x15 | Configura info do sistema |
| SET_REGISTRY | 0x16 | Configura registry |
| GSP_RM_ALLOC | 0x24 | Aloca objetos RM |
| GSP_RM_CONTROL | 0x25 | Controla objetos RM |
| GSP_INIT_DONE | 0x52 | Evento: GSP pronto |

## Para NVDAAL

### Próximos Passos:
1. Portar estruturas de dados (nv.h) para IOKit
2. Implementar alocação de sysmem DMA-able
3. Implementar comunicação RPC via MMIO
4. Baixar e integrar firmware GSP
5. Implementar sequência de boot

### Diferenças macOS vs Linux:
- IOKit em vez de /dev/nv*
- IOBufferMemoryDescriptor para DMA
- Sem mmap direto (usar IOMemoryMap)

## Referências

- [TinyGPU/tinygrad](https://github.com/tinygrad/tinygrad)
- [NVIDIA open-gpu-kernel-modules](https://github.com/NVIDIA/open-gpu-kernel-modules)
- [NVIDIA linux-firmware](https://github.com/NVIDIA/linux-firmware)
- [Nouveau/Mesa](https://nouveau.freedesktop.org/)
