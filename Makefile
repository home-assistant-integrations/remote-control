# remote-control firmware for the M5Paper.
#
# `make` installs the pinned toolchain into toolchain/ on the first run and
# then builds the firmware. Nothing is installed outside the repository,
# apart from the download caches of uv and git.

# Pinned versions. Changing one of them reinstalls the toolchain.
IDF_VERSION    := v5.5.6
PYTHON_VERSION := 3.12
IDF_TARGET     := esp32
# ESP-IDF installs CMake and Ninja on macOS and Linux only on request. Without
# them, the build would fall back to whatever is installed on the host.
EXTRA_TOOLS    := cmake ninja

# Serial port for flash and monitor, for example
# `make flash PORT=/dev/cu.usbserial-0001`. Empty: ESP-IDF detects the port.
PORT ?=

TOOLCHAIN_DIR := $(CURDIR)/toolchain
PYTHON_DIR    := $(TOOLCHAIN_DIR)/python
STAMP         := $(TOOLCHAIN_DIR)/.stamp
STAMP_CONTENT := idf=$(IDF_VERSION) python=$(PYTHON_VERSION) target=$(IDF_TARGET) tools=$(subst $(eval) ,+,$(EXTRA_TOOLS))
IDF_REPO      := https://github.com/espressif/esp-idf.git

# Override any ESP-IDF set in the user's shell.
export IDF_PATH                 := $(TOOLCHAIN_DIR)/esp-idf
export IDF_TOOLS_PATH           := $(TOOLCHAIN_DIR)/espressif
export IDF_COMPONENT_CACHE_PATH := $(TOOLCHAIN_DIR)/component-cache
export UV_PYTHON_INSTALL_DIR    := $(PYTHON_DIR)

SHELL := /bin/bash
HOST_TOOLS := git curl uv

ifneq ($(word 2,$(CURDIR)),)
$(error The project path "$(CURDIR)" contains a space. ESP-IDF does not support that. Move the project to a path without spaces)
endif

# Directory of the uv-managed Python. Evaluated when used, because it only
# exists once the toolchain is installed.
PYTHON_BIN = $(dir $(shell uv python find --managed-python $(PYTHON_VERSION) 2>/dev/null))

# Runs idf.py with the environment of the ESP-IDF in toolchain/.
IDF_PY = export PATH="$(PYTHON_BIN):$$PATH" && . "$(IDF_PATH)/export.sh" >/dev/null && idf.py $(if $(PORT),-p "$(PORT)")

.DEFAULT_GOAL := all
.PHONY: all build toolchain install-toolchain check-host-tools flash monitor menuconfig clean distclean help

all: build ## Install the toolchain if needed, then build

build: toolchain ## Build the firmware
	@$(IDF_PY) build

flash: toolchain ## Flash the firmware to the device on PORT
	@$(IDF_PY) flash

monitor: toolchain ## Open the serial monitor on PORT (exit with Ctrl-])
	@$(IDF_PY) monitor

menuconfig: toolchain ## Open the ESP-IDF configuration menu
	@$(IDF_PY) menuconfig

toolchain: ## Install the toolchain only (skipped if up to date)
	@if [ "$$(cat "$(STAMP)" 2>/dev/null)" = "$(STAMP_CONTENT)" ]; then \
		echo "Toolchain up to date ($(STAMP_CONTENT))"; \
	else \
		$(MAKE) --no-print-directory install-toolchain; \
	fi

check-host-tools:
	@for tool in $(HOST_TOOLS); do \
		command -v $$tool >/dev/null 2>&1 || { \
			echo "Error: '$$tool' is required but was not found on the PATH. Install it and run make again." >&2; \
			exit 1; \
		}; \
	done

install-toolchain: check-host-tools
	@echo "Installing toolchain ($(STAMP_CONTENT)) into $(TOOLCHAIN_DIR)"
	@rm -f "$(STAMP)"
	@mkdir -p "$(TOOLCHAIN_DIR)"
	@echo "==> Python $(PYTHON_VERSION)"
	@uv python install --no-bin $(PYTHON_VERSION)
	@echo "==> ESP-IDF $(IDF_VERSION)"
	@rm -rf "$(IDF_PATH)"
	@git -c advice.detachedHead=false clone --depth 1 --branch $(IDF_VERSION) \
		--recurse-submodules --shallow-submodules "$(IDF_REPO)" "$(IDF_PATH)"
	@echo "==> ESP-IDF tools for $(IDF_TARGET)"
	@export PATH="$(PYTHON_BIN):$$PATH" && "$(IDF_PATH)/install.sh" $(IDF_TARGET)
	@echo "==> $(EXTRA_TOOLS)"
	@export PATH="$(PYTHON_BIN):$$PATH" && python3 "$(IDF_PATH)/tools/idf_tools.py" install $(EXTRA_TOOLS)
	@echo "$(STAMP_CONTENT)" > "$(STAMP)"
	@echo "Toolchain installed."

clean: ## Remove build output, keep the toolchain
	rm -rf build

distclean: clean ## Remove build output, downloaded components and toolchain/
	rm -rf managed_components sdkconfig sdkconfig.old "$(TOOLCHAIN_DIR)"

help: ## List targets and variables
	@echo "Targets:"
	@grep -hE '^[a-z-]+:.*## ' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*## "} {printf "  %-12s %s\n", $$1, $$2}'
	@echo ""
	@echo "Variables:"
	@echo "  PORT         Serial port for flash and monitor (default: detected)"
	@echo ""
	@echo "Pinned: ESP-IDF $(IDF_VERSION), Python $(PYTHON_VERSION), target $(IDF_TARGET)"
