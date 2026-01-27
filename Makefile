# Makefile para NVDAAL.kext
# NVIDIA Ada Lovelace Compute Driver para macOS
# Foco: AI/ML - Sem suporte a display

KEXT_NAME = NVDAAL
BUNDLE_ID = com.nvdaal.compute
BUILD_DIR = Build
KEXT_PATH = $(BUILD_DIR)/$(KEXT_NAME).kext
INFO_PLIST = Info.plist

# Arquivos fonte
SOURCES = Sources/NVDAAL.cpp
# SOURCES += Sources/NVDAALGsp.cpp  # TODO: Integrar quando pronto

# SDK e ferramentas
SDKROOT = $(shell xcrun --show-sdk-path)
CXX = clang++

# Flags de compilação para kernel extension
CXXFLAGS = -arch x86_64 \
           -mmacosx-version-min=13.0 \
           -mkernel \
           -nostdinc \
           -fno-builtin \
           -fno-common \
           -fno-exceptions \
           -fno-rtti \
           -fno-stack-protector \
           -fstrict-aliasing \
           -O2 \
           -g \
           -DKERNEL \
           -DKERNEL_PRIVATE \
           -DDRIVER_PRIVATE \
           -DAPPLE \
           -DNeXT \
           -DNVDAAL_COMPUTE_ONLY=1

# Includes
INCLUDES = -I$(SDKROOT)/System/Library/Frameworks/Kernel.framework/Headers \
           -I$(SDKROOT)/System/Library/Frameworks/IOKit.framework/Headers \
           -ISources

# Flags de link
LDFLAGS = -arch x86_64 \
          -static \
          -nostdlib \
          -Wl,-kext \
          -Wl,-exported_symbols_list,/dev/null \
          -lkmod

# Objetos
OBJECTS = $(BUILD_DIR)/NVDAAL.o

# =============================================================================
# Targets
# =============================================================================

all: $(KEXT_PATH)
	@echo "[+] Build completo: $(KEXT_PATH)"

$(KEXT_PATH): $(BUILD_DIR) $(KEXT_PATH)/Contents/MacOS/$(KEXT_NAME) $(KEXT_PATH)/Contents/Info.plist

$(BUILD_DIR)/NVDAAL.o: Sources/NVDAAL.cpp Sources/NVDAALRegs.h
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(KEXT_PATH)/Contents/MacOS/$(KEXT_NAME): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "[*] Binary: $@"

$(KEXT_PATH)/Contents/Info.plist: $(INFO_PLIST)
	@mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# =============================================================================
# Comandos de desenvolvimento
# =============================================================================

clean:
	rm -rf $(BUILD_DIR)
	@echo "[*] Build limpo."

rebuild: clean all

# Verificar estrutura do kext
test: $(KEXT_PATH)
	@echo "[*] Verificando estrutura do kext..."
	@ls -la $(KEXT_PATH)/Contents/
	@ls -la $(KEXT_PATH)/Contents/MacOS/
	@echo "[*] Validando Info.plist..."
	@plutil $(KEXT_PATH)/Contents/Info.plist
	@echo "[*] Verificando binário..."
	@file $(KEXT_PATH)/Contents/MacOS/$(KEXT_NAME)
	@echo "[+] Estrutura OK!"

# =============================================================================
# Instalação e gerenciamento
# =============================================================================

# Instalar permanentemente (requer reboot)
install: $(KEXT_PATH)
	@echo "[*] Instalando NVDAAL.kext..."
	sudo chown -R root:wheel $(KEXT_PATH)
	sudo cp -R $(KEXT_PATH) /Library/Extensions/
	sudo touch /Library/Extensions/
	sudo kextcache -invalidate /
	@echo "[+] Kext instalado. Reboot necessário."
	@echo "[!] Certifique-se de ter boot-args: kext-dev-mode=1"

# Carregar temporariamente (para testes)
load: $(KEXT_PATH)
	@echo "[*] Carregando NVDAAL.kext..."
	sudo chown -R root:wheel $(KEXT_PATH)
	sudo kextload $(KEXT_PATH)
	@echo "[+] Kext carregado."
	@echo "[*] Verificar logs: make logs"

# Descarregar
unload:
	@echo "[*] Descarregando NVDAAL.kext..."
	-sudo kextunload -b $(BUNDLE_ID)
	@echo "[+] Kext descarregado."

# Reinstalar (unload + install)
reinstall: unload clean all install

# =============================================================================
# Debug e logs
# =============================================================================

# Ver logs do driver
logs:
	@echo "[*] Logs do NVDAAL (últimos 5 minutos):"
	log show --predicate 'eventMessage contains "NVDAAL"' --last 5m

# Ver logs em tempo real
logs-live:
	@echo "[*] Logs do NVDAAL (tempo real - Ctrl+C para sair):"
	log stream --predicate 'eventMessage contains "NVDAAL"'

# Status do kext
status:
	@echo "[*] Status do NVDAAL:"
	@kextstat | grep -i nvdaal || echo "Kext não carregado"
	@echo ""
	@echo "[*] Dispositivos PCI NVIDIA:"
	@ioreg -l | grep -i "class IOPCIDevice" -A 20 | grep -i nvidia || echo "Nenhum encontrado"

# =============================================================================
# Firmware
# =============================================================================

# Baixar firmware GSP
download-firmware:
	@echo "[*] Baixando firmware GSP..."
	@mkdir -p Firmware
	curl -L -o Firmware/gsp-570.144.bin \
		"https://github.com/NVIDIA/linux-firmware/raw/nvidia-staging/nvidia/ad102/gsp/gsp-570.144.bin" || \
	curl -L -o Firmware/gsp-570.144.bin \
		"https://github.com/NVIDIA/linux-firmware/raw/refs/heads/nvidia-staging/nvidia/ga102/gsp/gsp-570.144.bin"
	@ls -la Firmware/gsp-570.144.bin
	@echo "[+] Firmware baixado."

.PHONY: all clean rebuild test install load unload reinstall logs logs-live status download-firmware
