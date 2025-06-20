# 4x4 Mesh NoC Interrupt System

## Phase 1 Implementation

This directory contains the Phase 1 implementation of the interrupt system for the 4x4 mesh NoC platform. The system provides interrupt communication between tiles and the C0 master.

### Features

- **Interrupt Types**: Support for 8 different interrupt types (task complete, error, DMA complete, etc.)
- **Priority-based Processing**: 4 priority levels (critical, high, normal, low)
- **C0 Master Controller**: Centralized interrupt management on tile 0
- **Tile Interface**: Per-tile interrupt interface for tiles 1-7
- **Queue Management**: Circular buffers for IRQ queuing with thread safety
- **Statistics and Monitoring**: Comprehensive statistics tracking
- **Modular Communication**: Pluggable communication backend (Unix sockets implemented)

### Architecture

```
Tiles 1-7                    C0 Master (Tile 0)
┌─────────────┐             ┌─────────────────────┐
│ Tile        │   Send IRQ  │ Interrupt           │
│ Interface   │──────────►  │ Controller          │
│             │             │                     │
│ - Send IRQs │             │ - IRQ Queue         │
│ - Receive   │ ◄──────────  │ - ISR Handlers      │
│   Commands  │  Send Cmd   │ - Statistics        │
└─────────────┘             └─────────────────────┘
```

### File Structure

```
interrupt_system/
├── interrupt_types.h         # Core data structures and types
├── interrupt_controller.h    # C0 master controller interface  
├── interrupt_controller.c    # C0 master implementation
├── tile_interrupt.h          # Tile-side interface
├── tile_interrupt.c          # Tile-side implementation
├── interrupt_communication.h # Communication abstraction
├── interrupt_communication.c # Communication implementation
├── interrupt_system.h        # Unified API
├── interrupt_system.c        # Unified API implementation
├── Makefile                  # Build system
└── README.md                 # This file
```

### Building

```bash
# Build everything
make all

# Build static library only
make static

# Build with debug information
make debug

# Build optimized release
make release

# Clean build files
make clean

# View all available targets
make help
```

### Basic Usage

#### For C0 Master (Tile 0):

```c
#include "interrupt_system.h"

// Initialize as C0 master
interrupt_system_init(0, COMM_METHOD_UNIX_SOCKET);

// Start processing interrupts
interrupt_system_start_c0_processing();

// Register custom ISR
interrupt_system_register_c0_isr(IRQ_TYPE_TASK_COMPLETE, my_task_complete_handler);

// Send command to tile
interrupt_system_c0_send_to_tile(3, IRQ_TYPE_CUSTOM, 0x123, "Configure tile");

// Print statistics
interrupt_system_c0_print_statistics();

// Cleanup
interrupt_system_destroy();
```

#### For Tiles (1-7):

```c
#include "interrupt_system.h"

// Initialize as tile (e.g., tile 3)
interrupt_system_init(3, COMM_METHOD_UNIX_SOCKET);

// Start processing incoming interrupts
interrupt_system_start_tile_processing();

// Register ISR for incoming commands
interrupt_system_register_tile_isr(IRQ_TYPE_CUSTOM, my_command_handler);

// Send interrupt to C0
interrupt_system_tile_signal_task_complete(task_id);
interrupt_system_tile_signal_error(error_code, "Error description");
interrupt_system_tile_signal_dma_complete(transfer_id);

// Print statistics  
interrupt_system_tile_print_statistics();

// Cleanup
interrupt_system_destroy();
```

### Interrupt Types

| Type | Priority | Description |
|------|----------|-------------|
| `IRQ_TYPE_TASK_COMPLETE` | Normal | Task execution completed |
| `IRQ_TYPE_ERROR` | Critical | Error condition |
| `IRQ_TYPE_DMA_COMPLETE` | High | DMA transfer finished |
| `IRQ_TYPE_NOC_CONGESTION` | Low | Network congestion detected |
| `IRQ_TYPE_RESOURCE_REQUEST` | High | Resource allocation request |
| `IRQ_TYPE_CUSTOM` | Normal | Custom application interrupt |
| `IRQ_TYPE_TIMER` | Low | Timer expiration |
| `IRQ_TYPE_SHUTDOWN` | Critical | Shutdown request |

### Configuration

The system supports several configuration options:

- **MAX_PENDING_IRQS**: Maximum IRQs in C0 queue (default: 64)
- **MAX_INCOMING_IRQS**: Maximum IRQs in tile queue (default: 16)
- **MAX_TILES**: Number of tiles supported (default: 8)
- **Communication Method**: Currently Unix domain sockets

### Thread Safety

The interrupt system is fully thread-safe:

- All queue operations are protected by mutexes
- Condition variables signal IRQ availability  
- Separate processing threads handle IRQ dispatch
- Statistics are atomically updated

### Error Handling

The API uses consistent error codes:

- `IRQ_SYSTEM_SUCCESS` (0): Operation successful
- `IRQ_SYSTEM_ERROR_INVALID` (-1): Invalid argument
- `IRQ_SYSTEM_ERROR_NOT_INIT` (-2): System not initialized
- `IRQ_SYSTEM_ERROR_PERMISSION` (-4): Operation not allowed
- etc.

### Phase 1 Limitations

This Phase 1 implementation includes:

✅ **Completed**:
- Core interrupt data structures
- C0 master interrupt controller
- Tile interrupt interfaces  
- Unix socket communication
- Thread-safe queue management
- Statistics and monitoring
- Unified API

🚧 **Phase 1 Limitations**:
- Communication is logging-only (no actual socket connections yet)
- No integration with existing HAL/task system
- No shared memory communication method
- No advanced features (coalescing, throttling)

### Testing

Simple test programs can be built:

```bash
# Build test programs
make tests

# Run C0 test (in one terminal)
./test_c0

# Run tile test (in another terminal)  
./test_tile
```

### Next Phases

**Phase 2** will add:
- Full socket communication implementation
- Integration with existing HAL/task system
- Real interrupt transmission between processes

**Phase 3** will add:
- Advanced features (interrupt coalescing, throttling)
- Performance optimization
- Additional communication methods
- Comprehensive test suite

### Integration Points

To integrate with the existing NoC platform:

1. **Include headers**: Add `#include "interrupt_system/interrupt_system.h"`
2. **Initialize**: Call `interrupt_system_init()` during startup
3. **Replace manual coordination**: Use interrupt system instead of polling
4. **Add to build**: Link with `-linterrupt_system`

The system is designed to complement the existing HAL and task infrastructure without requiring major changes to the core platform. 