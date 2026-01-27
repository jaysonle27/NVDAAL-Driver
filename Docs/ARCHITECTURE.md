# Arquitetura do NVDAAL Driver

## Visão Geral

```
+--------------------------------------------------+
|              macOS Tahoe 26                       |
|  +----------------+  +-----------------------+    |
|  |   Metal Apps   |  |  Legacy OpenGL (opc)  |    |
|  |   (via shim)   |  |     (Mesa/libGL)      |    |
|  +-------+--------+  +-----------+-----------+    |
|          |                       |                |
|  +-------v-----------------------+-----------+    |
|  |       MetalNVIDIA.framework (shim)        |    |
|  |   - Traduz Metal → GPU commands           |    |
|  +-------+------------------------------+----+    |
|          |                              |         |
|  +-------v--------+    +---------------v---+      |
|  | libnouveau-mac |    | OpenCore + ACPI   |      |
|  |  (user-space)  |    | patches (SSDT)    |      |
|  +-------+--------+    +-------------------+      |
|          |                                        |
|  +-------v---------------------------------+      |
|  |          NVDAAL.kext (IOKit)            |      |
|  |  - PCI init, VBIOS load, MMIO, IRQs     |      |
|  +-------+---------------------------------+      |
|          |                                        |
+----------|----------------------------------------+
           |
+----------v------------------+  +------------------+
|   RTX 4090 (Ada Lovelace)   |  |   Firmware       |
|   - PCI ID: 10DE:2684       |  |   (VBIOS + blobs)|
|   - GDDR6X, DP/HDMI         |  |   - Extraído/TPU |
+-----------------------------+  +------------------+
```

## Camadas

### 1. NVDAAL.kext (Kernel Extension)

Driver de kernel baseado em IOKit. Responsável por:

- **PCI Enumeration**: Detectar GPU via Device ID
- **MMIO Mapping**: Mapear registros da GPU na memória
- **VBIOS Loading**: Carregar firmware de inicialização
- **Interrupt Handling**: Gerenciar IRQs (MSI-X)
- **Memory Management**: Alocar e gerenciar VRAM

### 2. libnouveau-mac (User-Space Driver)

Port do driver Mesa/nouveau adaptado para macOS:

- **Command Submission**: Enviar comandos para a GPU
- **Shader Compilation**: Compilar shaders para PTX/SASS
- **Buffer Management**: Gerenciar buffers de vértices/texturas
- **Fence/Sync**: Sincronização CPU-GPU

### 3. MetalNVIDIA.framework (Metal Shim)

Camada de compatibilidade Metal:

- **MTLDevice**: Wrapper para device NVIDIA
- **MTLCommandQueue**: Fila de comandos
- **MTLRenderPipelineState**: Estado de renderização
- **MTLTexture**: Texturas e framebuffers

## Registros da GPU

### BAR0 - MMIO Registers

| Offset | Nome | Descrição |
|--------|------|-----------|
| 0x0 | BOOT0 | Status de boot |
| 0x88000 | PBUS | Controle do barramento |
| 0x9200 | PMC | Power management |
| 0x610000 | PDISP | Display controller |
| 0x1400 | PTIMER | Timer da GPU |

### Sequência de Inicialização

1. **Probe**: Verificar Device ID (0x10DE:0x2684)
2. **Start**: Habilitar bus master e memory
3. **Map MMIO**: Mapear BAR0
4. **Load VBIOS**: Carregar firmware
5. **Init PMU**: Inicializar power management
6. **Configure Clocks**: Definir frequências
7. **Enable Display**: Configurar saída de vídeo

## Injeção de VBIOS via OpenCore

```xml
<key>DeviceProperties</key>
<dict>
    <key>Add</key>
    <dict>
        <key>PciRoot(0x0)/Pci(0x1,0x0)/Pci(0x0,0x0)</key>
        <dict>
            <key>rom</key>
            <data><!-- Base64 do VBIOS --></data>
            <key>device-id</key>
            <data>hCYAAA==</data>
        </dict>
    </dict>
</dict>
```

## Desafios Técnicos

1. **GSP Firmware**: Ada Lovelace usa GSP (GPU System Processor) que requer firmware assinado
2. **SASS Shaders**: Binários de shader são proprietários
3. **Power Management**: Controle de voltagem/frequência é complexo
4. **Display**: Configuração de timing/EDID para monitores

## Referências

- [NVIDIA Open GPU Kernel Modules](https://github.com/NVIDIA/open-gpu-kernel-modules)
- [Nouveau GPU Documentation](https://envytools.readthedocs.io/)
- [IOKit Fundamentals](https://developer.apple.com/documentation/iokit)
