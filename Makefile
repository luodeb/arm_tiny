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

# Source files
C_SOURCES = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/virtio/*.c)
ASM_SOURCES = $(wildcard $(ASM_DIR)/*.S)

# Object files  
C_OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OUTPUT_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst $(ASM_DIR)/%.S, $(OUTPUT_DIR)/%.o, $(ASM_SOURCES))
OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

# Output file
TARGET = arm_tiny

# Compiler flags
CFLAGS = -Wall -I$(INCLUDE_DIR) -c -lc -g -O0 -fno-pie -fno-builtin-printf -mgeneral-regs-only -DVM_VERSION=\"$(if $(VM_VERSION),$(VM_VERSION),"null")\"
LDFLAGS = -T link.lds

# Build rules
all: $(OUTPUT_DIR) $(OUTPUT_DIR)/$(TARGET).bin

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)
	mkdir -p $(OUTPUT_DIR)/virtio

$(OUTPUT_DIR)/$(TARGET).bin: $(OUTPUT_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOL_PREFIX)objdump -x -d -S $(OUTPUT_DIR)/$(TARGET).elf > $(OUTPUT_DIR)/$(TARGET)_dis.txt
	$(TOOL_PREFIX)readelf -a $(OUTPUT_DIR)/$(TARGET).elf  > $(OUTPUT_DIR)/$(TARGET)_elf.txt


$(OUTPUT_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

$(OUTPUT_DIR)/%.o: $(ASM_DIR)/%.S
	$(CC) $(CFLAGS) -o $@ $<
	
$(OUTPUT_DIR)/$(TARGET).elf: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

debug:
	qemu-system-aarch64 -m 4G -M virt -cpu cortex-a72 -nographic -kernel $(OUTPUT_DIR)/$(TARGET).elf -s -S

run:
	qemu-system-aarch64 -m 4G -M virt -cpu cortex-a72 -nographic -kernel $(OUTPUT_DIR)/$(TARGET).elf

virtio_test:
	qemu-system-aarch64 -m 4G -M virt -cpu cortex-a72 \
	-nographic -kernel $(OUTPUT_DIR)/$(TARGET).elf \
	-device virtio-blk-device,drive=test \
	-drive file=test.img,if=none,id=test,format=raw,cache=none

mutil_uart:
	/home/debin/Tools/qemu-9.1.2/aarch64/bin/qemu-system-aarch64 -m 4G -M virt -cpu cortex-a72 -nographic -kernel $(OUTPUT_DIR)/$(TARGET).elf -serial mon:stdio -serial telnet:localhost:4321,server -serial telnet:localhost:4322,server -serial telnet:localhost:4323,server

clean:
	rm -rf $(OUTPUT_DIR)

.PHONY: all clean