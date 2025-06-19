CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -pthread -ldl -Itile -I. -Imesh_noc -Idmem -I..

# Gather all C sources
SRCS := $(shell find . -name '*.c')
OBJS := $(SRCS:.c=.o)

TARGET := soc_top

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	@echo ">> Running..."
	@./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
