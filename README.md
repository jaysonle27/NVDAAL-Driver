# NVDAAL - NVIDIA Ada Lovelace Driver for macOS Hackintosh

Driver open source para GPUs NVIDIA RTX 40 series (Ada Lovelace) no macOS Hackintosh.

## Status

🚧 **EM DESENVOLVIMENTO** - Este projeto está em estágio inicial.

## Objetivo

Criar um driver funcional para a RTX 4090 (e outras GPUs Ada Lovelace) no macOS Tahoe (26), permitindo:

- [ ] Detecção e inicialização da GPU
- [ ] Framebuffer básico (saída de vídeo)
- [ ] Aceleração 2D
- [ ] Suporte básico ao Metal
- [ ] Power management

## Hardware Suportado

| GPU | Device ID | Status |
|-----|-----------|--------|
| RTX 4090 | 0x2684 | 🔴 Em desenvolvimento |
| RTX 4090 D | 0x2685 | 🔴 Planejado |
| RTX 4080 Super | 0x2702 | 🔴 Planejado |
| RTX 4080 | 0x2704 | 🔴 Planejado |
| RTX 4070 Ti Super | 0x2705 | 🔴 Planejado |

## Requisitos

### Hardware
- PC com suporte a Hackintosh (Intel ou AMD)
- GPU NVIDIA RTX 40 series
- macOS Tahoe 26 (via OpenCore)

### Software
- Xcode Command Line Tools
- OpenCore 1.0.7+
- Máquina Linux (para extração de VBIOS)
- Python 3.6+

## Instalação

### 1. Extrair VBIOS (no Linux)

```bash
cd Tools/
sudo python3 extract_vbios.py -o vbios_4090.rom
```

### 2. Compilar o kext (no macOS)

```bash
make
make test  # Valida estrutura
```

### 3. Instalar

```bash
make install
```

### 4. Configurar OpenCore

Adicione ao `config.plist`:

```xml
<key>Kernel</key>
<dict>
    <key>Add</key>
    <array>
        <dict>
            <key>BundlePath</key>
            <string>NVDAAL.kext</string>
            <key>Enabled</key>
            <true/>
            <key>ExecutablePath</key>
            <string>Contents/MacOS/NVDAAL</string>
            <key>PlistPath</key>
            <string>Contents/Info.plist</string>
        </dict>
    </array>
</dict>
```

## Estrutura do Projeto

```
NVDAAL-Driver/
├── README.md
├── LICENSE
├── Makefile
├── Info.plist
├── Sources/
│   └── NVDAAL.c          # Driver principal
├── Firmware/
│   └── README.md         # Instruções para VBIOS
├── Tools/
│   └── extract_vbios.py  # Extrator de VBIOS
├── Docs/
│   ├── ARCHITECTURE.md
│   ├── DEBUGGING.md
│   └── TODO.md
└── Build/                # Kext gerado aqui
```

## Recursos Utilizados

- [NVIDIA open-gpu-kernel-modules](https://github.com/NVIDIA/open-gpu-kernel-modules)
- [Nouveau Project](https://nouveau.freedesktop.org/)
- [envytools](https://github.com/envytools/envytools)
- [TechPowerUp VBIOS Collection](https://www.techpowerup.com/vgabios/)

## Comunidade

- [InsanelyMac](https://www.insanelymac.com/)
- [r/hackintosh](https://www.reddit.com/r/hackintosh/)
- [OpenCore Docs](https://dortania.github.io/)

## Aviso Legal

Este projeto é para fins educacionais e de pesquisa. Não há garantia de funcionamento.
O uso de firmware proprietário pode violar termos de licença da NVIDIA.
Use por sua conta e risco.

## Licença

MIT License - Veja [LICENSE](LICENSE)

## Contribuindo

Pull requests são bem-vindos! Por favor, leia [CONTRIBUTING.md](Docs/CONTRIBUTING.md) primeiro.
