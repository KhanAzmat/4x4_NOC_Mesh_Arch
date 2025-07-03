CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -pthread -ldl -Itile -I. -Imesh_noc -Idmem -I.. -Iplatform_init -Ihal/INT -Ihal/dma512

# Gather all C sources for the platform (including DMAC512 and PLIC HAL)
SRCS := $(shell find . -name '*.c' | grep -v -E 'hal/INT/(aradlm|printf)\.c')
OBJS := $(SRCS:.c=.o)

TARGET := soc_top

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	@echo ">> Running 4x4 Mesh NoC Platform with Integrated Interrupt System..."
	@./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
