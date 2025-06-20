# Interrupt System Makefile
# Phase 1 implementation

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -fPIC -g -O2
LDFLAGS = -pthread

# Library target
LIB_NAME = libinterrupt_system
STATIC_LIB = $(LIB_NAME).a
SHARED_LIB = $(LIB_NAME).so.1.0

# Source files
SRCS = interrupt_types.h \
       interrupt_controller.c \
       tile_interrupt.c \
       interrupt_communication.c \
       interrupt_system.c

# Object files
OBJS = interrupt_controller.o \
       tile_interrupt.o \
       interrupt_communication.o \
       interrupt_system.o

# Header files
HEADERS = interrupt_types.h \
          interrupt_controller.h \
          tile_interrupt.h \
          interrupt_communication.h \
          interrupt_system.h

# Default target
all: $(STATIC_LIB) $(SHARED_LIB)

# Static library
$(STATIC_LIB): $(OBJS)
	@echo "Creating static library: $@"
	ar rcs $@ $^

# Shared library
$(SHARED_LIB): $(OBJS)
	@echo "Creating shared library: $@"
	$(CC) -shared -Wl,-soname,$(LIB_NAME).so.1 -o $@ $^ $(LDFLAGS)
	ln -sf $(SHARED_LIB) $(LIB_NAME).so.1
	ln -sf $(SHARED_LIB) $(LIB_NAME).so

# Object files
%.o: %.c $(HEADERS)
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Simple test programs (for development)
test_c0: test_c0.c $(STATIC_LIB)
	@echo "Building C0 test program"
	$(CC) $(CFLAGS) -o $@ $< -L. -linterrupt_system $(LDFLAGS)

test_tile: test_tile.c $(STATIC_LIB)
	@echo "Building tile test program"
	$(CC) $(CFLAGS) -o $@ $< -L. -linterrupt_system $(LDFLAGS)

# Install targets
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include/interrupt_system

install: $(STATIC_LIB) $(SHARED_LIB)
	@echo "Installing interrupt system..."
	install -d $(LIBDIR)
	install -d $(INCDIR)
	install -m 644 $(STATIC_LIB) $(LIBDIR)/
	install -m 755 $(SHARED_LIB) $(LIBDIR)/
	ln -sf $(LIBDIR)/$(SHARED_LIB) $(LIBDIR)/$(LIB_NAME).so.1
	ln -sf $(LIBDIR)/$(SHARED_LIB) $(LIBDIR)/$(LIB_NAME).so
	install -m 644 $(HEADERS) $(INCDIR)/
	ldconfig

uninstall:
	@echo "Uninstalling interrupt system..."
	rm -f $(LIBDIR)/$(STATIC_LIB)
	rm -f $(LIBDIR)/$(SHARED_LIB)
	rm -f $(LIBDIR)/$(LIB_NAME).so.1
	rm -f $(LIBDIR)/$(LIB_NAME).so
	rm -rf $(INCDIR)
	ldconfig

# Clean targets
clean:
	@echo "Cleaning build files..."
	rm -f $(OBJS)
	rm -f $(STATIC_LIB) $(SHARED_LIB)
	rm -f $(LIB_NAME).so.1 $(LIB_NAME).so
	rm -f test_c0 test_tile
	rm -f /tmp/noc_interrupt_*.sock

distclean: clean
	@echo "Deep cleaning..."
	rm -f *~ *.bak *.orig
	rm -f core core.*

# Development targets
debug: CFLAGS += -DDEBUG -g3 -O0
debug: all

release: CFLAGS += -DNDEBUG -O3
release: all

# Check for common issues
check:
	@echo "Checking for common issues..."
	@echo "Checking for proper includes..."
	@grep -n "#include" *.c *.h || true
	@echo "Checking for memory leaks potential..."
	@grep -n "malloc\|free\|calloc\|realloc" *.c || true
	@echo "Checking for thread safety..."
	@grep -n "pthread_\|static\|global" *.c *.h || true

# Static analysis (if available)
analyze:
	@echo "Running static analysis..."
	@which cppcheck > /dev/null 2>&1 && cppcheck --enable=all --std=c99 *.c || echo "cppcheck not available"
	@which clang-tidy > /dev/null 2>&1 && clang-tidy *.c -- $(CFLAGS) || echo "clang-tidy not available"

# Format code (if available)
format:
	@echo "Formatting code..."
	@which clang-format > /dev/null 2>&1 && clang-format -i *.c *.h || echo "clang-format not available"

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build both static and shared libraries (default)"
	@echo "  static       - Build static library only"
	@echo "  shared       - Build shared library only"
	@echo "  test_c0      - Build C0 test program"
	@echo "  test_tile    - Build tile test program"
	@echo "  install      - Install libraries and headers"
	@echo "  uninstall    - Remove installed files"
	@echo "  clean        - Remove build files"
	@echo "  distclean    - Remove all generated files"
	@echo "  debug        - Build with debug flags"
	@echo "  release      - Build with optimization flags"
	@echo "  check        - Check for common issues"
	@echo "  analyze      - Run static analysis"
	@echo "  format       - Format source code"
	@echo "  help         - Show this help"

# Alternative targets
static: $(STATIC_LIB)
shared: $(SHARED_LIB)
tests: test_c0 test_tile

# Phony targets
.PHONY: all clean distclean install uninstall debug release check analyze format help static shared tests

# Dependencies
interrupt_controller.o: interrupt_controller.c interrupt_controller.h interrupt_types.h
tile_interrupt.o: tile_interrupt.c tile_interrupt.h interrupt_types.h
interrupt_communication.o: interrupt_communication.c interrupt_communication.h interrupt_types.h
interrupt_system.o: interrupt_system.c interrupt_system.h interrupt_controller.h tile_interrupt.h interrupt_communication.h interrupt_types.h 