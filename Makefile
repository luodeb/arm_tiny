# Makefile for ARM Tiny project

TOOL_PREFIX=aarch64-none-linux-gnu-
# TOOL_PREFIX=aarch64-linux-musl-

# Compiler and tools
CC = $(TOOL_PREFIX)gcc
LD = $(TOOL_PREFIX)ld
OBJCOPY = $(TOOL_PREFIX)objcopy

# Directories
INCLUDE_DIR = include
SRC_DIR = src
ASM_DIR = asm
OUTPUT_DIR = build
DISK_IMG := test.img
LOG ?= info

# Source files
C_SOURCES = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/virtio/*.c)
ASM_SOURCES = $(wildcard $(ASM_DIR)/*.S)

# Object files
C_OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OUTPUT_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst $(ASM_DIR)/%.S, $(OUTPUT_DIR)/%.o, $(ASM_SOURCES))
OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

# Output file
TARGET = arm_tiny

# QEMU 配置
QEMU = qemu-system-aarch64
QEMU_ARGS = -m 4G -M virt -cpu cortex-a72 \
	-nographic -kernel $(OUTPUT_DIR)/$(TARGET).elf \
	-device virtio-blk-device,drive=test \
	-drive file=test.img,if=none,id=test,format=raw,cache=none \
	-device virtio-net-device,netdev=net0,mac=00:00:00:00:00:03 \
	-netdev user,id=net0,hostfwd=tcp::5555-:5555,hostfwd=udp::5555-:5555

# Log level mapping
LOG_LEVEL_none = 0
LOG_LEVEL_error = 1
LOG_LEVEL_warn = 2
LOG_LEVEL_info = 3
LOG_LEVEL_debug = 4
LOG_LEVEL_all = 5

# Get the numeric log level
LOG_LEVEL_NUM = $(LOG_LEVEL_$(LOG))

# Compiler flags
CFLAGS = -Wall -I$(INCLUDE_DIR) -c -lc -g -O0 -fno-pie -fno-builtin-printf -mgeneral-regs-only \
	-DVM_VERSION=\"$(if $(VM_VERSION),$(VM_VERSION),"null")\" \
	-DLOG_LEVEL=$(if $(LOG_LEVEL_NUM),$(LOG_LEVEL_NUM),3)
LDFLAGS = -T link.lds

# Build rules
all: $(OUTPUT_DIR) $(OUTPUT_DIR)/$(TARGET).bin
	@echo "$(GREEN_C)Build completed$(END_C) with LOG level: $(GREEN_C)$(LOG)$(END_C)"

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR)/virtio

$(OUTPUT_DIR)/$(TARGET).bin: $(OUTPUT_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOL_PREFIX)objdump -x -d -S $(OUTPUT_DIR)/$(TARGET).elf > $(OUTPUT_DIR)/$(TARGET)_dis.txt
	$(TOOL_PREFIX)readelf -a $(OUTPUT_DIR)/$(TARGET).elf  > $(OUTPUT_DIR)/$(TARGET)_elf.txt

$(OUTPUT_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "$(BLUE_C)Compiling$(END_C) $<"
	@$(CC) $(CFLAGS) -o $@ $<

$(OUTPUT_DIR)/%.o: $(ASM_DIR)/%.S
	@echo "$(BLUE_C)Assembling$(END_C) $<"
	@$(CC) $(CFLAGS) -o $@ $<
	
$(OUTPUT_DIR)/$(TARGET).elf: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

debug: all
	@echo "Starting $(TARGET) in QEMU with GDB support..."
	@echo "Connect with: gdb-multiarch -ex 'target remote :1234' $(TARGET).elf"
	@echo "Press Ctrl+A then X to exit QEMU"
	$(QEMU) $(QEMU_ARGS) -s -S

run: all
	@echo "Starting $(TARGET) in QEMU..."
	@echo "Press Ctrl+A then X to exit QEMU"
	$(QEMU) $(QEMU_ARGS)

disk_img:
	@printf "    $(GREEN_C)Creating$(END_C) FAT32 disk image \"$(DISK_IMG)\" ...\n"
	@dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64
	@mkfs.fat -F 32 $(DISK_IMG)

clean:
	@echo "$(YELLOW_C)Cleaning$(END_C) build directory..."
	rm -rf $(OUTPUT_DIR)

.PHONY: all clean run debug disk_img log-help help