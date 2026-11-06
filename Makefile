# Switch File Manager — build sem alterações nos fontes
.SUFFIXES:
ifeq ($(strip $(DEVKITPRO)),)
$(error "Defina DEVKITPRO no ambiente.")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

TARGET := SwitchFileManager
BUILD := build
SOURCES := src/app src/core src/ui src/storage src/cloud src/network src/archives src/diagnostics src/security src/updates third_party/miniz
DATA := data
INCLUDES := include third_party/miniz third_party/stb

APP_TITLE := Switch File Manager
APP_AUTHOR := TemudjinBR
override APP_VERSION := $(shell python3 $(TOPDIR)/scripts/version.py)
ifeq ($(strip $(APP_VERSION)),)
$(error VERSION inválido ou indisponível)
endif

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS += $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS := -g $(ARCH)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lnx
LIBDIRS := $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
ifneq ($(words $(CPPFILES) $(CFILES)),$(words $(sort $(CPPFILES) $(CFILES))))
$(error Nomes de fontes duplicados entre módulos; use nomes únicos.)
endif
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)/generated
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_ICON := $(if $(wildcard $(CURDIR)/assets/icon.jpg),$(CURDIR)/assets/icon.jpg,$(DEVKITPRO)/libnx/default_icon.jpg)
export NROFLAGS += --icon=$(APP_ICON) --nacp=$(CURDIR)/$(TARGET).nacp

.PHONY: $(BUILD) clean all
all: $(BUILD)
ifeq ($(TARGET),SwitchFileManager)
	@$(MAKE) --no-print-directory TARGET=SfmUpdateHelper BUILD=build-helper SOURCES="tools/updater src/updates src/core src/network src/security src/diagnostics src/cloud" CPPFILES="updater_main.cpp update_core.cpp" CFILES="" INCLUDES="include build/generated" APP_TITLE="Atualizador do Switch File Manager"
endif
$(BUILD):
	@python3 $(CURDIR)/scripts/prepare.py
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf build-helper SfmUpdateHelper.nro SfmUpdateHelper.nacp SfmUpdateHelper.elf
else
.PHONY: all
DEPENDS := $(OFILES:.o=.d)
all: $(OUTPUT).nro
$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)
.PHONY: FORCE_NACP
# Conferir o conteúdo em todo build também cobre arquivos copiados com timestamps antigos.
$(OUTPUT).nacp: FORCE_NACP $(TOPDIR)/VERSION $(TOPDIR)/scripts/version.py
	@nacptool --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi
-include $(DEPENDS)
endif
