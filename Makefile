CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -pthread -ldl -Itile -I. -Imesh_noc -Idmem -I..

# Gather all C sources for the platform (excluding hal directory)
SRCS := $(shell find . -name '*.c' -not -path './hal/*')

# Add specific HAL sources we want to include
SRCS += hal/dma512/hal_dmac512.c

# Create object file list from all sources
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
