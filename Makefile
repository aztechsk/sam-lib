.DEFAULT_GOAL := all

TOP := $(abspath ..)
include $(TOP)/mk/common.mk

PROJECT := ucdrv
OUT     := $(BUILD_ROOT)/$(PROJECT)
TARGET  := $(OUT)/$(PROJECT).a

SRCS := \
	src/adc.c \
	src/btn.c \
	src/btn1.c \
	src/chipid.c \
	src/criterr.c \
	src/dacc.c \
	src/dlycnt.c \
	src/eefc.c \
	src/gpio_hal_impl.c \
	src/hsmci_sd.c \
	src/hwerr.c \
	src/i2c.c \
	src/led.c \
	src/ledui.c \
	src/matrix.c \
	src/pinmon.c \
	src/pinmux_hal_impl.c \
	src/pio.c \
	src/pmc.c \
	src/rstc.c \
	src/shift164.c \
	src/shift165.c \
	src/sleep.c \
	src/spi.c \
	src/spi_hal_impl.c \
	src/supc.c \
	src/tc.c \
	src/uart.c \
	src/usart.c \
	src/wd.c

OBJS := $(addprefix $(OUT)/,$(notdir $(SRCS:.c=.o)))
DEPS := $(OBJS:.o=.d)

SYS_INCLUDES := \
	-isystem$(CMSIS3_INC) \
	-isystem$(TOP)/freertos/src/inc

USER_INCLUDES := \
	-I$(TOP)/ucdrv/inc \
	-I$(TOP)/inc \
	-I$(TOP)/sys/src \
	-I$(TOP)/hal/src/inc

WARNINGS := \
	-Wall \
	-Wextra \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wshadow \
	-Wpointer-arith \
	-Wbad-function-cast \
	-Wcast-align \
	-Wcast-qual \
	-Wjump-misses-init \
	-Wno-unused-parameter \
	-Wno-unused-but-set-variable \
	-Wundef

OPT_FLAGS := -O1
CC1_FLAGS := $(CW_CC1_PRE_FLAGS) $(SYS_INCLUDES) $(USER_INCLUDES) $(CW_COMMON_DEFS)
CC1_POST_FLAGS := $(CW_CC1_BASE_FLAGS) $(WARNINGS) $(OPT_FLAGS) $(CW_CODEGEN_FLAGS)
AS_FLAGS := --traditional-format $(USER_INCLUDES) $(CPU_AS_FLAGS)

.PHONY: all clean
all: check-toolchain $(TARGET)

$(TARGET): $(OBJS)
	@echo "  AR      $@"
	@rm -f "$@"
	$(AR) -rcs "$@" $(OBJS)

$(OUT)/%.o: src/%.c | $(OUT)
	@echo "  CC      $<"
	$(CC1) $(CC1_FLAGS) -MD "$(OUT)/$*.d" -MQ "$@" $(CC1_POST_FLAGS) "$<" -o "$(OUT)/$*.asm"
	$(AS) $(AS_FLAGS) "$(OUT)/$*.asm" -o "$@"
	@rm -f "$(OUT)/$*.asm"

$(OUT):
	@mkdir -p "$@"

clean:
	@rm -rf "$(OUT)"

-include $(DEPS)
