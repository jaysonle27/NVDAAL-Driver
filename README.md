# NVDAAL - NVIDIA Ada Lovelace Compute Driver for macOS

Driver open source para **compute/AI/ML** em GPUs NVIDIA RTX 40 series (Ada Lovelace) no macOS.

## Status

**EM DESENVOLVIMENTO** - Estágio inicial focado em inicialização GSP.

## Objetivo

Driver de **compute-only** para RTX 4090 no macOS, permitindo:

- [x] Detecção e inicialização da GPU
- [ ] Inicialização do GSP (GPU System Processor)
- [ ] Alocação de memória GPU (VRAM)
- [ ] Compute queues para AI/ML workloads
- [ ] Interface para frameworks de ML (tinygrad, PyTorch, etc)

**Nota**: Este driver **NÃO** suporta display/vídeo. Focamos 100% em compute.

## Por que Compute-Only?

1. **Simplicidade**: Sem framebuffer, display engine, HDMI/DP = código muito menor
2. **Foco**: RTX 4090 é uma fera para AI/ML, aproveitamos isso
3. **Viabilidade**: TinyGPU provou que compute funciona no macOS
4. **Performance**: Toda a potência vai para compute, não para display

## Hardware Suportado

| GPU | Device ID | Tensor Cores | Status |
|-----|-----------|--------------|--------|
| RTX 4090 | 0x2684 | 512 | Em desenvolvimento |
| RTX 4090 D | 0x2685 | 512 | Planejado |
| RTX 4080 Super | 0x2702 | 320 | Planejado |
| RTX 4080 | 0x2704 | 304 | Planejado |
| RTX 4070 Ti Super | 0x2705 | 264 | Planejado |

## Arquitetura

```
+------------------------------------------+
|          Aplicações AI/ML                |
|  (tinygrad, PyTorch, TensorFlow, etc)    |
+------------------+-----------------------+
                   |
+------------------v-----------------------+
|         NVDAAL User Library              |
|   - Alocação de buffers                  |
|   - Submissão de compute kernels         |
|   - Sincronização CPU-GPU                |
+------------------+-----------------------+
                   |
+------------------v-----------------------+
|           NVDAAL.kext (IOKit)            |
|   - GSP initialization                   |
|   - Memory management (VRAM)             |
|   - Compute queue management             |
|   - RPC communication with GSP           |
+------------------+-----------------------+
                   |
+------------------v-----------------------+
|      RTX 4090 (Ada Lovelace AD102)       |
|   - 16384 CUDA cores                     |
|   - 512 Tensor cores (4th gen)           |
|   - 24GB GDDR6X                          |
|   - GSP (RISC-V processor)               |
+------------------------------------------+
```

## Requisitos

### Hardware
- Hackintosh (Intel ou AMD)
- GPU NVIDIA RTX 40 series
- macOS Tahoe 26+ (via OpenCore)

### Software
- Xcode Command Line Tools
- OpenCore 1.0.7+

## Compilação

```bash
make clean && make
make test  # Valida estrutura
```

## Instalação

```bash
# Temporário (para testes)
make load

# Permanente
make install
# Reboot necessário
```

## Logs

```bash
log show --predicate 'eventMessage contains "NVDAAL"' --last 5m
```

## Baseado em

- [TinyGPU/tinygrad](https://github.com/tinygrad/tinygrad) - Referência principal para GSP
- [NVIDIA open-gpu-kernel-modules](https://github.com/NVIDIA/open-gpu-kernel-modules)
- [Nouveau Project](https://nouveau.freedesktop.org/)

## Roadmap

1. **Fase 1**: GSP Initialization (atual)
2. **Fase 2**: Memory Management
3. **Fase 3**: Compute Queues
4. **Fase 4**: User-space Library
5. **Fase 5**: Framework Integration

## Licença

MIT License - Veja [LICENSE](LICENSE)

## Aviso

Projeto educacional/pesquisa. Use por sua conta e risco.
