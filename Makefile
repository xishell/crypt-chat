# DTEK-V Project Makefile
# Improved with debug/release builds and proper dependencies

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
TOOL_DIR := ../tools

# Toolchain
TOOLCHAIN := riscv32-unknown-elf-
CC := $(TOOLCHAIN)gcc
LD := $(TOOLCHAIN)ld
OBJCOPY := $(TOOLCHAIN)objcopy
OBJDUMP := $(TOOLCHAIN)objdump

# Source files
SOURCES := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*.S)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SOURCES))) \
           $(patsubst $(SRC_DIR)/%.S,$(BUILD_DIR)/%.o,$(filter %.S,$(SOURCES)))

# Linker script
LINKER := dtekv-script.lds

# Compiler flags
ARCH_FLAGS := -mabi=ilp32 -march=rv32imzicsr
COMMON_FLAGS := -Wall -nostdlib -fno-builtin -I$(INC_DIR)
CFLAGS_RELEASE := $(COMMON_FLAGS) $(ARCH_FLAGS) -O3
CFLAGS_DEBUG := $(COMMON_FLAGS) $(ARCH_FLAGS) -O0 -g -DDEBUG

# Default to release build
BUILD_TYPE ?= release
ifeq ($(BUILD_TYPE),debug)
    CFLAGS := $(CFLAGS_DEBUG)
    TARGET := $(BUILD_DIR)/main_debug
else
    CFLAGS := $(CFLAGS_RELEASE)
    TARGET := $(BUILD_DIR)/main
endif

# Targets
.PHONY: all clean debug release run upload help

all: $(TARGET).bin

# Build rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	@echo "AS $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS) softfloat.a
	@echo "LD $@"
	@$(LD) -o $@ -T $(LINKER) $(filter-out $(BUILD_DIR)/boot.o,$(OBJECTS)) softfloat.a

$(TARGET).bin: $(TARGET).elf
	@echo "OBJCOPY $@"
	@$(OBJCOPY) --output-target binary $< $@
	@$(OBJDUMP) -D $< > $<.txt
	@echo "Build complete: $@"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Convenience targets
debug:
	@$(MAKE) BUILD_TYPE=debug all

release:
	@$(MAKE) BUILD_TYPE=release all

# Upload and run
run: all
	@echo "Uploading and running..."
	@bash run_lab.sh

upload: run

# Clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f *.o *.elf *.bin *.txt
	@echo "Clean complete"

# Help
help:
	@echo "DTEK-V Project Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build the project (default: release)"
	@echo "  debug        Build with debug symbols and no optimization"
	@echo "  release      Build with optimizations (default)"
	@echo "  run          Build and upload to DTEK-V board"
	@echo "  upload       Same as run"
	@echo "  clean        Remove build artifacts"
	@echo "  help         Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_TYPE   Set to 'debug' or 'release' (default: release)"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build release version"
	@echo "  make debug              # Build debug version"
	@echo "  make run                # Build and upload"
	@echo "  make BUILD_TYPE=debug   # Build debug explicitly"

# Dependency tracking
-include $(OBJECTS:.o=.d)

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@$(CC) $(CFLAGS) -MM -MT $(BUILD_DIR)/$*.o $< > $@

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.S | $(BUILD_DIR)
	@$(CC) $(CFLAGS) -MM -MT $(BUILD_DIR)/$*.o $< > $@
