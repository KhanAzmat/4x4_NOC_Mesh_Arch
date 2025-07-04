CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -pthread -ldl -Itile -I. -Imesh_noc -Idmem -I.. -Iplatform_init -Ihal/INT -Ihal/dma512

# Linker flags – wrap the HAL functions so that the simulation fabric can
# run immediately after the real HAL implementation.
LDFLAGS := -Wl,--wrap=PLIC_N_source_pending_write \
           -Wl,--wrap=PLIC_M_TAR_comp_write     \
           -Wl,--wrap=PLIC_M_TAR_claim_read

# Gather all C sources for the platform (including DMAC512 and PLIC HAL)
SRCS := $(shell find . -name '*.c' | grep -v -E 'hal/INT/(aradlm|printf)\.c')
OBJS := $(SRCS:.c=.o)

TARGET := soc_top

SRC_C0_MASTER = c0_master/c0_controller.c \
                c0_master/c0_master.c \
                platform_init/system_setup.c \
                platform_init/address_manager.c \
                platform_init/dmac512_hardware_monitor.c \
                platform_init/plic_sim_bridge.c \
                hal_tests/test_framework.c \
                hal_tests/hal_test_tasks.c \
                hal_tests/dmac512_comprehensive_tests.c

# List of all source files for tile processors
# ... existing code ...

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	@echo ">> Running 4x4 Mesh NoC Platform with Integrated Interrupt System..."
	@./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
