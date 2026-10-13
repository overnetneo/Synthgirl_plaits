# Makefile fuer Synthgirl-App-H723 (ersetzt das STM32CubeIDE-Projekt fuer Kommandozeilen-Builds)
TARGET  = Synthgirl-App-H723
BUILD   = build
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
CXX     = $(PREFIX)g++
AS      = $(PREFIX)gcc -x assembler-with-cpp
CP      = $(PREFIX)objcopy
SZ      = $(PREFIX)size

# Plaits-Engine ein-/ausschalten (make PLAITS=0 baut die Original-Firmware)
PLAITS ?= 1
# Diagnose-Firmware (make DIAG=1): Fehler/Haenger werden gespeichert und angezeigt
DIAG ?= 0

MCU   = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
DEFS  = -DUSE_HAL_DRIVER -DSTM32H723xx -DUSE_PWR_LDO_SUPPLY
OPT   = -Ofast

INCS = -ICore/Inc -ICore/Library/Controller -ICore/Library/Dac -ICore/Library/Global -ICore/Library/Lcd \
       -IDrivers/CMSIS/Device/ST/STM32H7xx/Include -IDrivers/CMSIS/Include \
       -IDrivers/STM32H7xx_HAL_Driver/Inc -IDrivers/STM32H7xx_HAL_Driver/Inc/Legacy \
       -IFATFS/App -IFATFS/Target -IMiddlewares/Third_Party/FatFs/src

C_SRC   = $(wildcard Core/Src/*.c) $(wildcard Drivers/STM32H7xx_HAL_Driver/Src/*.c) \
          $(wildcard FATFS/App/*.c) $(wildcard FATFS/Target/*.c) \
          $(wildcard Middlewares/Third_Party/FatFs/src/*.c) $(wildcard Middlewares/Third_Party/FatFs/src/option/*.c)
CXX_SRC = $(wildcard Core/Src/*.cpp) $(filter-out Core/Library/PlaitsVoice/%,$(wildcard Core/Library/*/*.cpp))
ASM_SRC = Core/Startup/startup_stm32h723zgtx.s

ifeq ($(DIAG),1)
DEFS    += -DSG_DIAG=1
endif

ifeq ($(PLAITS),1)
DEFS    += -DSG_PLAITS=1 -DSG_FIX_SAMPLERATE=1
INCS    += -IThird_Party/eurorack -ICore/Library/PlaitsVoice
CXX_SRC += $(wildcard Core/Library/PlaitsVoice/*.cpp) \
           $(wildcard Third_Party/eurorack/plaits/dsp/*.cc) \
           $(wildcard Third_Party/eurorack/plaits/dsp/*/*.cc) \
           Third_Party/eurorack/plaits/resources.cc \
           Third_Party/eurorack/stmlib/dsp/units.cc \
           Third_Party/eurorack/stmlib/utils/random.cc
endif

CFLAGS   = $(MCU) $(DEFS) $(INCS) $(OPT) -g3 -ffunction-sections -fdata-sections -Wall -Wno-unused -fstack-usage
CXXFLAGS = $(CFLAGS) -std=gnu++14 -fno-lifetime-dse -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
LDFLAGS  = $(MCU) -TSTM32H723ZGTX_FLASH.ld --specs=nosys.specs --specs=nano.specs -Wl,--gc-sections \
           -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage -lstdc++ -lsupc++ -lm

OBJS = $(addprefix $(BUILD)/,$(C_SRC:.c=.o)) $(addprefix $(BUILD)/,$(patsubst %.cc,%.o,$(CXX_SRC:.cpp=.o))) \
       $(addprefix $(BUILD)/,$(ASM_SRC:.s=.o))

all: $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -std=gnu11 $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -c $(MCU) $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(CP) -O binary -S $< $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
