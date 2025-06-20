#ifndef INTERRUPT_SYSTEM_H
#define INTERRUPT_SYSTEM_H

// Main interrupt system API - unified interface
// This header includes all necessary components and provides a simple API

#include "interrupt_types.h"
#include "interrupt_controller.h"
#include "tile_interrupt.h"
#include "interrupt_communication.h"

// Global interrupt system state
typedef struct {
    bool system_initialized;
    
    // C0 Master components (only valid on C0)
    interrupt_controller_t* c0_controller;
    interrupt_communication_t* c0_comm;
    
    // Tile components (only valid on tiles 1-7)
    tile_interrupt_t* tile_interface;
    interrupt_communication_t* tile_comm;
    
    // Current entity information
    int entity_id;              // 0 for C0, 1-7 for tiles
    bool is_c0_master;          // True if this is C0 master
    
    // System configuration
    communication_method_t comm_method;
    bool enable_statistics;
    bool enable_debug;
    
} interrupt_system_t;

// Global interrupt system instance (singleton)
extern interrupt_system_t g_interrupt_system;

// ============================================================================
// MAIN API FUNCTIONS
// ============================================================================

// System initialization and cleanup
int interrupt_system_init(int entity_id, communication_method_t comm_method);
int interrupt_system_destroy(void);

// Check if system is initialized and ready
bool interrupt_system_is_ready(void);
int interrupt_system_get_entity_id(void);
bool interrupt_system_is_c0_master(void);

// ============================================================================
// C0 MASTER API (only available when entity_id == 0)
// ============================================================================

// Start/stop C0 interrupt processing
int interrupt_system_start_c0_processing(void);
int interrupt_system_stop_c0_processing(void);

// Register ISR handlers on C0
int interrupt_system_register_c0_isr(interrupt_type_t type, interrupt_isr_t handler);
int interrupt_system_unregister_c0_isr(interrupt_type_t type);

// C0 control functions
int interrupt_system_c0_enable_tile(int tile_id);
int interrupt_system_c0_disable_tile(int tile_id);
int interrupt_system_c0_enable_type(interrupt_type_t type);
int interrupt_system_c0_disable_type(interrupt_type_t type);

// C0 monitoring
int interrupt_system_c0_get_queue_count(void);
int interrupt_system_c0_get_queue_space(void);
void interrupt_system_c0_print_statistics(void);
void interrupt_system_c0_reset_statistics(void);

// Send IRQs from C0 to tiles
int interrupt_system_c0_send_to_tile(int target_tile, interrupt_type_t type, 
                                     uint32_t data, const char* message);

// ============================================================================
// TILE API (only available when entity_id >= 1)
// ============================================================================

// Start/stop tile interrupt processing
int interrupt_system_start_tile_processing(void);
int interrupt_system_stop_tile_processing(void);

// Register ISR handlers on tile for incoming IRQs from C0
int interrupt_system_register_tile_isr(interrupt_type_t type, tile_interrupt_isr_t handler);
int interrupt_system_unregister_tile_isr(interrupt_type_t type);

// Tile control functions
int interrupt_system_tile_enable_incoming_type(interrupt_type_t type);
int interrupt_system_tile_disable_incoming_type(interrupt_type_t type);

// Tile monitoring
int interrupt_system_tile_get_incoming_queue_count(void);
int interrupt_system_tile_get_incoming_queue_space(void);
void interrupt_system_tile_print_statistics(void);
void interrupt_system_tile_reset_statistics(void);

// Send IRQs from tile to C0 (convenience functions)
int interrupt_system_tile_send_to_c0(interrupt_type_t type, uint32_t data, const char* message);
int interrupt_system_tile_signal_task_complete(uint32_t task_id);
int interrupt_system_tile_signal_error(uint32_t error_code, const char* error_msg);
int interrupt_system_tile_signal_dma_complete(uint32_t transfer_id);
int interrupt_system_tile_request_resource(uint32_t resource_type);
int interrupt_system_tile_signal_shutdown(void);

// ============================================================================
// COMMON API (available on both C0 and tiles)
// ============================================================================

// System configuration
int interrupt_system_enable_debug(bool enable);
int interrupt_system_enable_statistics(bool enable);

// Communication statistics
void interrupt_system_print_comm_statistics(void);
void interrupt_system_reset_comm_statistics(void);

// System status and diagnostics
void interrupt_system_print_status(void);
int interrupt_system_self_test(void);

// ============================================================================
// ERROR CODES
// ============================================================================

#define IRQ_SYSTEM_SUCCESS           0
#define IRQ_SYSTEM_ERROR_INVALID     -1
#define IRQ_SYSTEM_ERROR_NOT_INIT    -2
#define IRQ_SYSTEM_ERROR_ALREADY     -3
#define IRQ_SYSTEM_ERROR_PERMISSION  -4
#define IRQ_SYSTEM_ERROR_COMM        -5
#define IRQ_SYSTEM_ERROR_QUEUE_FULL  -6
#define IRQ_SYSTEM_ERROR_TIMEOUT     -7
#define IRQ_SYSTEM_ERROR_SYSTEM      -8

// Helper function to get error string
const char* interrupt_system_strerror(int error_code);

// ============================================================================
// CONVENIENCE MACROS
// ============================================================================

// Check if we're on C0 master
#define IS_C0_MASTER() (interrupt_system_is_c0_master())

// Check if we're on a tile
#define IS_TILE() (!interrupt_system_is_c0_master() && interrupt_system_get_entity_id() > 0)

// Send common IRQ types with automatic error handling
#define TILE_SIGNAL_TASK_COMPLETE(task_id) \
    do { \
        if (IS_TILE()) { \
            interrupt_system_tile_signal_task_complete(task_id); \
        } \
    } while(0)

#define TILE_SIGNAL_ERROR(code, msg) \
    do { \
        if (IS_TILE()) { \
            interrupt_system_tile_signal_error(code, msg); \
        } \
    } while(0)

#define TILE_SIGNAL_DMA_COMPLETE(id) \
    do { \
        if (IS_TILE()) { \
            interrupt_system_tile_signal_dma_complete(id); \
        } \
    } while(0)

#endif // INTERRUPT_SYSTEM_H 