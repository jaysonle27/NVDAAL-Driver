# Firmware para NVDAAL

Este diretório deve conter os arquivos de firmware extraídos da GPU.

## Arquivos Necessários

| Arquivo | Descrição | Fonte |
|---------|-----------|-------|
| `vbios.rom` | VBIOS extraído da GPU | `extract_vbios.py` |
| `gsp.bin` | Firmware GSP (opcional) | Linux driver |
| `acr.bin` | ACR firmware (opcional) | Linux driver |

## Como Extrair

### VBIOS (Método 1 - Linux direto)

```bash
cd ../Tools
sudo python3 extract_vbios.py -o ../Firmware/vbios.rom
```

### VBIOS (Método 2 - TechPowerUp)

1. Acesse https://www.techpowerup.com/vgabios/
2. Busque por "RTX 4090"
3. Baixe a versão compatível com sua placa
4. Salve como `vbios.rom` neste diretório

### GSP Firmware (do driver Linux)

```bash
# No Linux com driver NVIDIA instalado
cp /lib/firmware/nvidia/gsp_ga10x.bin ./gsp.bin
```

## Injeção via OpenCore

Para injetar o VBIOS via OpenCore:

1. Converta para Base64:
```bash
base64 -i vbios.rom -o vbios.txt
```

2. Adicione ao config.plist:
```xml
<key>DeviceProperties</key>
<dict>
    <key>Add</key>
    <dict>
        <key>PciRoot(0x0)/Pci(0x1,0x0)/Pci(0x0,0x0)</key>
        <dict>
            <key>rom</key>
            <data><!-- Cole o conteúdo de vbios.txt aqui --></data>
        </dict>
    </dict>
</dict>
```

## Aviso Legal

O uso de firmware proprietário pode violar os termos de licença da NVIDIA.
Este projeto é para fins educacionais e de pesquisa.
