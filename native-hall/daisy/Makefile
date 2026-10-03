.DEFAULT_GOAL := all
TARGET = Cineol224_Daisy
LIBDAISY_DIR ?= ../../build/deps/libDaisy
LIBDAISY_BUILD = $(abspath ../../build/daisy/libdaisy)
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
PROFILE ?= ../../build/rom/native-hall/programs-v44.bank224
NATIVE_HALL_ROM_DIR ?= $(abspath ../../firmware)/224 v4_4
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(shell python3 tools/verify_roms.py "$(NATIVE_HALL_ROM_DIR)" >/dev/null && echo verified),verified)
$(error Original Lexicon 224 v4.4 ROM1-ROM5 are required to build or flash this firmware)
endif
endif
DAISY_BOARD ?= seed
ifeq ($(DAISY_BOARD),patch-sm)
C_DEFS += -DCINEOL_DAISY_PATCH_SM
else ifneq ($(DAISY_BOARD),seed)
$(error DAISY_BOARD must be seed or patch-sm)
endif
APP_TYPE = BOOT_QSPI
LDSCRIPT = src/qspi_itcm.lds
CPP_SOURCES = src/main.cpp ../core/hall.cpp
override C_SOURCES = src/qspi_startup.c
CPP_STANDARD = -std=gnu++17
OPT = -O3 -ffp-contract=off
C_INCLUDES += -Isrc -I$(BUILD_DIR)
LIBDIR += -L$(LIBDAISY_BUILD)
include $(SYSTEM_FILES_DIR)/Makefile
$(BUILD_DIR)/qspi_startup.o: OPT := -Os
CPPFLAGS += -fno-threadsafe-statics -fno-use-cxa-atexit
.PHONY: libdaisy layout flash
libdaisy:
	$(MAKE) -C "$(LIBDAISY_DIR)" TARGET=libdaisy BUILD_DIR="$(LIBDAISY_BUILD)" OPT="-O3 -ffp-contract=off" MAKEOVERRIDES=
all: libdaisy layout
$(BUILD_DIR)/$(TARGET).elf: libdaisy src/qspi_itcm.lds
$(BUILD_DIR)/main.o: $(BUILD_DIR)/generated_profile.hpp
$(BUILD_DIR)/main.o: $(wildcard src/HardwareAdapter.local.hpp src/ControlConfig.local.hpp)
$(BUILD_DIR)/generated_profile.hpp: $(PROFILE) tools/embed_profile.py | $(BUILD_DIR)
	python3 tools/embed_profile.py "$(PROFILE)" "$@"
layout: $(BUILD_DIR)/$(TARGET).bin
	python3 tools/check_layout.py "$(BUILD_DIR)/$(TARGET).elf" "$(BUILD_DIR)/$(TARGET).bin"
flash: all
	@echo "Build verified. Press RESET normally; waiting for the Daisy bootloader..."
	dfu-util -w -a 0 -s $(QSPI_ADDRESS):leave -D "$(BUILD_DIR)/$(TARGET).bin" -d ,0483:$(DAISY_PID)
