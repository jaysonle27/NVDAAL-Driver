# TODO - NVDAAL Compute Driver

## Fase 1: Estrutura Base (COMPLETO)

- [x] Criar estrutura do projeto
- [x] Implementar kext básico com detecção PCI
- [x] Mapear BARs (MMIO + VRAM)
- [x] Ler registros de identificação (BOOT0, BOOT42)
- [x] Documentar arquitetura compute-only
- [x] Criar headers de registros (NVDAALRegs.h)
- [x] Criar estrutura GSP (NVDAALGsp.h/cpp)

## Fase 2: GSP Initialization (ATUAL)

- [ ] Baixar firmware GSP (gsp-570.144.bin)
- [ ] Implementar parser ELF para firmware
- [ ] Implementar alocação DMA (IOBufferMemoryDescriptor)
- [ ] Construir radix3 page table
- [ ] Configurar WPR metadata
- [ ] Implementar boot sequence:
  - [ ] Reset FALCON
  - [ ] Executar FWSEC (do VBIOS)
  - [ ] Iniciar RISC-V core
  - [ ] Aguardar GSP_INIT_DONE
- [ ] Implementar RPC básico:
  - [ ] Enqueue command
  - [ ] Dequeue status
  - [ ] sendRpc()
  - [ ] waitRpcResponse()

## Fase 3: Memory Management

- [ ] Implementar alocador de VRAM
- [ ] Suporte a buffers DMA
- [ ] Page table management
- [ ] Memory mapping para user-space
- [ ] Fence/sync objects

## Fase 4: Compute Queues

- [ ] Criar channel GPFIFO
- [ ] Implementar ring buffer
- [ ] Push buffer management
- [ ] Compute class instantiation (ADA_COMPUTE_A)
- [ ] Command submission
- [ ] Sync primitives

## Fase 5: User-Space Library

- [ ] IOUserClient para comunicação kernel<->user
- [ ] libNVDAAL.dylib
- [ ] API para:
  - [ ] Alocação de buffers
  - [ ] Upload/download de dados
  - [ ] Submissão de kernels
  - [ ] Sincronização

## Fase 6: Framework Integration

- [ ] Integração com tinygrad
- [ ] Backend PyTorch (optional)
- [ ] Documentação de API
- [ ] Exemplos de uso

## Notas de Implementação

### Prioridades
1. GSP é **obrigatório** - sem ele nada funciona em Ada Lovelace
2. Foco em compute queues, não display
3. Simplicidade > features

### Recursos Úteis
- TinyGPU: `tinygrad/runtime/support/nv/ip.py`
- NVIDIA open-gpu-kernel-modules
- Nouveau project

### Diferenças macOS
- IOBufferMemoryDescriptor em vez de dma_alloc_coherent
- IOUserClient em vez de /dev/nvidia*
- Sem mmap direto - usar IOMemoryMap

## Comandos Úteis

```bash
# Compilar
make clean && make

# Testar estrutura
make test

# Carregar temporariamente
make load

# Ver logs
make logs

# Ver logs em tempo real
make logs-live

# Baixar firmware
make download-firmware
```
