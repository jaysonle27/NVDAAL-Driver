# TODO - NVDAAL Driver

## Fase 1: Preparação (Atual)

- [x] Criar estrutura do projeto
- [x] Implementar kext básico com detecção PCI
- [x] Criar script de extração de VBIOS
- [x] Documentar arquitetura
- [ ] Extrair e analisar VBIOS da RTX 4090
- [ ] Estudar registros via envytools
- [ ] Analisar nvidia open-gpu-kernel-modules

## Fase 2: Inicialização Básica

- [ ] Carregar VBIOS via kext
- [ ] Implementar leitura de registros BOOT0/PMC
- [ ] Configurar interrupções MSI-X
- [ ] Mapear VRAM (BAR1)
- [ ] Inicializar PMU básico

## Fase 3: Display Output

- [ ] Detectar conectores (DP/HDMI)
- [ ] Ler EDID do monitor
- [ ] Configurar timing de vídeo
- [ ] Implementar framebuffer linear
- [ ] Teste: exibir imagem estática

## Fase 4: 2D Acceleration

- [ ] Implementar blitting
- [ ] Suporte a cópia de superfície
- [ ] Aceleração de cursor
- [ ] Teste: desktop com cursor acelerado

## Fase 5: Metal Básico

- [ ] Criar MTLDevice wrapper
- [ ] Implementar MTLCommandBuffer
- [ ] Suporte a texturas simples
- [ ] Pipeline de renderização básico
- [ ] Teste: triângulo em Metal

## Fase 6: Estabilidade

- [ ] Tratamento de erros robusto
- [ ] Power management (sleep/wake)
- [ ] Hot plug de display
- [ ] Debugging avançado
- [ ] Documentação completa

## Bloqueadores Conhecidos

1. **GSP Firmware**: Necessário para inicialização completa
2. **Shader Compiler**: Não existe compilador SASS open source
3. **NVLink**: Não suportado inicialmente
4. **Ray Tracing**: Fora do escopo inicial

## Recursos Necessários

- [ ] Máquina Linux com RTX 4090 para extração
- [ ] Analisador lógico para debug de timing
- [ ] Acesso a envytools modificado para Ada
- [ ] Tempo de engenharia reversa
