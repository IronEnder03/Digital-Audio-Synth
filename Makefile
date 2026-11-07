TARGET = main

# Toolchain definitions
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# Build Directory
BUILD_DIR = build
DEVICE_DRIVER_DIR = drivers/CMSIS/Device/ST/STM32F4xx

# Source Files (FIXED: Changed capitalized 'Drivers' to lowercase 'drivers')
C_SOURCES = src/main.c \
$(DEVICE_DRIVER_DIR)/Source/Templates/system_stm32f4xx.c \
src/lcd.c src/util.c src/midi_input.c src/audio_out.c 
ASM_SOURCES = $(DEVICE_DRIVER_DIR)/Source/Templates/gcc/startup_stm32f446xx.s

C_SOURCES += \
drivers/CMSIS/DSP/Source/BasicMathFunctions/BasicMathFunctions.c \
drivers/CMSIS/DSP/Source/CommonTables/CommonTables.c \
drivers/CMSIS/DSP/Source/TransformFunctions/TransformFunctions.c \
drivers/CMSIS/DSP/Source/FastMathFunctions/FastMathFunctions.c \
drivers/CMSIS/DSP/Source/FilteringFunctions/FilteringFunctions.c \
drivers/CMSIS/DSP/Source/SupportFunctions/SupportFunctions.c

# MCU Flags (Cortex-M4 with Hardware Floating Point Units)
MCU = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16

# Macros needed by CMSIS (ADDED: -DDISABLEFLOAT16 to prevent compilation errors)
DEFINES = -DSTM32F446xx -DARM_MATH_CM4 -DDISABLEFLOAT16

# Includes (ADDED: -Idrivers/CMSIS/DSP/PrivateInclude)
INCLUDES = \
-Iinclude \
-Idrivers/CMSIS/Include \
-Idrivers/CMSIS/DSP/Include \
-Idrivers/CMSIS/DSP/PrivateInclude \
-Idrivers/CMSIS/Device/ST/STM32F4xx/Include

# Compiler Flags
CFLAGS = $(MCU) $(DEFINES) $(INCLUDES) -O0 -Wall -Wextra -std=c11 -fdata-sections -ffunction-sections

# Linker Flags
LINKER_SCRIPT = STM32F446RETX_FLASH.ld
LDFLAGS = $(MCU) -T$(LINKER_SCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map --specs=nano.specs -lm -lc

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
