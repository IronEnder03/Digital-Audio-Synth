TARGET = main

# Toolchain definitions
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# Build Directory
BUILD_DIR = build

# Source Files
C_SOURCES = src/main.c src/system_stm32f4xx.c src/lcd.c src/util.c
ASM_SOURCES = drivers/Device/startup_stm32f446xx.s

# MCU Flags (Cortex-M4 with Hardware Floating Point Units)
MCU = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16

# Macros needed by CMSIS
DEFINES = -DSTM32F446xx

# Includes
INCLUDES = \
-Iinclude \
-Idrivers/CMSIS/CMSIS_6-6.3.0/CMSIS/Core/Include \
-Idrivers/CMSIS/CMSIS-DSP-1.18.0/Include \
-Idrivers/Device

# Compiler Flags
CFLAGS = $(MCU) $(DEFINES) $(INCLUDES) -O0 -Wall -Wextra -std=c11 -fdata-sections -ffunction-sections

# Linker Flags
LINKER_SCRIPT = STM32F446RETX_FLASH.ld
LDFLAGS = $(MCU) -T$(LINKER_SCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map --specs=nano.specs

# Generate Object paths
OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

# Rules
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

# Flash utility (Optional, requires stlink-tools installed)
flash: all
	st-flash write $(BUILD_DIR)/$(TARGET).bin 0x08000000

erase:
	st-flash --connect-under-reset erase

.PHONY: all clean flash erase

