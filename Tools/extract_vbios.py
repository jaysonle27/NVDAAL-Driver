#!/usr/bin/env python3
"""
extract_vbios.py - Extrai VBIOS de GPU NVIDIA via PCI usando Linux.

Baseado em extract-firmware-nouveau.py (projeto nouveau).
Execute como root em uma máquina Linux com a GPU instalada.

Usage:
    sudo python3 extract_vbios.py -o vbios_4090.rom
"""

import os
import sys
import struct
import argparse

PCI_BUS_PATH = "/sys/bus/pci/devices"

def find_nvidia_gpus():
    """Encontra todas as GPUs NVIDIA no sistema."""
    gpus = []

    if not os.path.exists(PCI_BUS_PATH):
        print("[!] Caminho PCI não encontrado. Este script deve ser executado no Linux.")
        return gpus

    for device_dir in os.listdir(PCI_BUS_PATH):
        vendor_path = os.path.join(PCI_BUS_PATH, device_dir, "vendor")
        device_path = os.path.join(PCI_BUS_PATH, device_dir, "device")

        if not (os.path.exists(vendor_path) and os.path.exists(device_path)):
            continue

        try:
            with open(vendor_path, 'r') as f:
                vendor = f.read().strip()
            with open(device_path, 'r') as f:
                device = f.read().strip()

            if vendor == "0x10de":  # NVIDIA
                gpus.append((device_dir, device))
        except Exception:
            continue

    return gpus


def get_device_name(device_id):
    """Retorna nome do dispositivo baseado no Device ID."""
    devices = {
        "0x2684": "RTX 4090",
        "0x2685": "RTX 4090 D",
        "0x2702": "RTX 4080 Super",
        "0x2704": "RTX 4080",
        "0x2705": "RTX 4070 Ti Super",
        "0x2782": "RTX 4070 Ti",
        "0x2786": "RTX 4070",
        "0x2860": "RTX 4070 Super",
    }
    return devices.get(device_id.lower(), f"Unknown ({device_id})")


def read_vbios_from_device(pci_addr):
    """Lê o VBIOS diretamente do dispositivo PCI."""
    rom_path = f"/sys/bus/pci/devices/{pci_addr}/rom"

    if not os.path.exists(rom_path):
        print(f"[!] ROM não encontrada em {rom_path}")
        return None

    try:
        # Enable ROM read
        with open(rom_path, 'wb') as f:
            f.write(b"1")

        # Read ROM data
        with open(rom_path, 'rb') as f:
            data = f.read()

        # Disable ROM read
        with open(rom_path, 'wb') as f:
            f.write(b"0")

        return data
    except PermissionError:
        print(f"[!] Permissão negada. Execute como root (sudo).")
        return None
    except Exception as e:
        print(f"[!] Erro ao ler VBIOS em {pci_addr}: {e}")
        return None


def validate_vbios(data):
    """Valida se os dados parecem ser um VBIOS válido."""
    if not data or len(data) < 64:
        return False, "Dados muito pequenos"

    # Check for ROM signature (55 AA)
    if data[0:2] != b'\x55\xAA':
        return False, "Assinatura ROM inválida (esperado 55 AA)"

    # Check for NVIDIA signature in PCI data structure
    # Usually at offset 0x18-0x1A points to PCI data
    pci_offset = struct.unpack('<H', data[0x18:0x1A])[0]
    if pci_offset + 4 <= len(data):
        pci_sig = data[pci_offset:pci_offset+4]
        if pci_sig == b'PCIR':
            return True, "VBIOS válido (PCIR encontrado)"

    return True, "VBIOS parece válido (assinatura 55 AA)"


def save_vbios(data, output_file):
    """Salva o VBIOS em um arquivo."""
    valid, message = validate_vbios(data)
    print(f"[*] Validação: {message}")

    if not valid:
        print("[!] VBIOS pode estar corrompido ou inválido")
        response = input("Deseja salvar mesmo assim? (y/n): ")
        if response.lower() != 'y':
            return False

    with open(output_file, 'wb') as f:
        f.write(data)

    size_kb = len(data) / 1024
    print(f"[+] VBIOS salvo em: {output_file} ({size_kb:.1f} KB)")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Extrai VBIOS de GPU NVIDIA para uso no Hackintosh",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Exemplos:
    sudo python3 extract_vbios.py                    # Extrai da primeira GPU
    sudo python3 extract_vbios.py -o my_4090.rom     # Nome customizado
    sudo python3 extract_vbios.py -l                 # Lista GPUs disponíveis
    sudo python3 extract_vbios.py -i 1               # Extrai da segunda GPU

Após extrair:
    1. Copie o arquivo .rom para o Hackintosh
    2. Converta para Base64 se necessário para OpenCore
    3. Injete via DeviceProperties no config.plist
        """
    )
    parser.add_argument("-o", "--output",
                        help="Arquivo de saída (ex: vbios_4090.rom)",
                        default="vbios.rom")
    parser.add_argument("-l", "--list",
                        action="store_true",
                        help="Lista GPUs NVIDIA disponíveis")
    parser.add_argument("-i", "--index",
                        type=int,
                        default=0,
                        help="Índice da GPU (0 = primeira)")

    args = parser.parse_args()

    print("[*] NVDAAL VBIOS Extractor")
    print("[*] Procurando GPUs NVIDIA...")

    gpus = find_nvidia_gpus()

    if not gpus:
        print("[!] Nenhuma GPU NVIDIA encontrada.")
        print("[!] Certifique-se de que:")
        print("    - Este script está rodando no Linux")
        print("    - A GPU NVIDIA está instalada")
        print("    - Você tem permissões de root")
        sys.exit(1)

    print(f"\n[*] {len(gpus)} GPU(s) NVIDIA encontrada(s):")
    for i, (addr, dev_id) in enumerate(gpus):
        name = get_device_name(dev_id)
        print(f"    {i}. {addr} - {name} (Device ID: {dev_id})")

    if args.list:
        sys.exit(0)

    if args.index >= len(gpus):
        print(f"\n[!] Índice {args.index} inválido. Use -l para listar GPUs.")
        sys.exit(1)

    addr, dev_id = gpus[args.index]
    name = get_device_name(dev_id)

    print(f"\n[*] Extraindo VBIOS de {name} ({addr})...")
    data = read_vbios_from_device(addr)

    if not data:
        print("[!] Falha ao extrair VBIOS")
        sys.exit(1)

    if save_vbios(data, args.output):
        print(f"\n[+] Sucesso!")
        print(f"[*] Próximos passos:")
        print(f"    1. Copie {args.output} para seu Hackintosh")
        print(f"    2. Adicione ao OpenCore via DeviceProperties")
        print(f"    3. Use o script inject_vbios.py para gerar config")


if __name__ == "__main__":
    main()
