#ifndef TILE_INTERRUPT_H
#define TILE_INTERRUPT_H

#include <pthread.h>
#include "interrupt_types.h"

// Forward declaration for tile ISR function pointer type
typedef int (*tile_interrupt_isr_t)(interrupt_request_t* irq);

// Tile-side interrupt interface - handles communication with C0 master
typedef struct {
    int tile_id;                       // This tile's ID (1-7)
    
    // Incoming IRQ queue from C0 master (circular buffer)
    interrupt_request_t incoming_queue[MAX_INCOMING_IRQS];
    int incoming_head;                 // Next position to read from
    int incoming_tail;                 // Next position to write to  
    int incoming_count;                // Current number of IRQs in queue
    pthread_mutex_t incoming_lock;     // Protects incoming queue
    pthread_cond_t incoming_available; // Signals when incoming IRQ available
    
    // IRQ masking and control
    bool incoming_irq_enabled[IRQ_TYPE_MAX + 1]; // Enable/disable per type
    bool interrupt_enabled;            // Master enable/disable for this tile
    
    // Interrupt Service Routine handlers for incoming IRQs from C0
    tile_interrupt_isr_t incoming_isr_handlers[IRQ_TYPE_MAX + 1];
    
    // Statistics
    uint64_t irqs_sent_to_c0;          // Total IRQs sent to C0 master
    uint64_t irqs_received_from_c0;    // Total IRQs received from C0 master
    uint64_t irqs_dropped_incoming;    // Incoming IRQs dropped (queue full)
    uint64_t irqs_masked_incoming;     // Incoming IRQs dropped (masked)
    uint64_t send_failures;            // Failed attempts to send to C0
    
    // Processing thread for incoming IRQs
    pthread_t incoming_processor_thread;
    bool incoming_processor_running;
    
} tile_interrupt_t;

// Tile interrupt interface functions
int tile_interrupt_init(tile_interrupt_t* tile_irq, int tile_id);
int tile_interrupt_destroy(tile_interrupt_t* tile_irq);

// Sending IRQs to C0 master
int tile_send_interrupt_to_c0(tile_interrupt_t* tile_irq, interrupt_type_t type, uint32_t data, const char* message);

// Receiving IRQs from C0 master
int tile_receive_interrupt_from_c0(tile_interrupt_t* tile_irq, interrupt_request_t* irq);
int tile_process_incoming_queue(tile_interrupt_t* tile_irq);
void* tile_incoming_processor_thread_main(void* arg);

// ISR management for incoming IRQs
void tile_register_incoming_isr(tile_interrupt_t* tile_irq, interrupt_type_t type, tile_interrupt_isr_t handler);
void tile_unregister_incoming_isr(tile_interrupt_t* tile_irq, interrupt_type_t type);

// Control and configuration
int tile_enable_interrupts(tile_interrupt_t* tile_irq);
int tile_disable_interrupts(tile_interrupt_t* tile_irq);
int tile_enable_incoming_type(tile_interrupt_t* tile_irq, interrupt_type_t type);
int tile_disable_incoming_type(tile_interrupt_t* tile_irq, interrupt_type_t type);

// Utility and monitoring
int tile_incoming_queue_space_available(tile_interrupt_t* tile_irq);
int tile_incoming_queue_count(tile_interrupt_t* tile_irq);
void tile_print_interrupt_statistics(tile_interrupt_t* tile_irq);
void tile_reset_interrupt_statistics(tile_interrupt_t* tile_irq);

// Convenience functions for common IRQ types
int tile_signal_task_complete(tile_interrupt_t* tile_irq, uint32_t task_id);
int tile_signal_error(tile_interrupt_t* tile_irq, uint32_t error_code, const char* error_msg);
int tile_signal_dma_complete(tile_interrupt_t* tile_irq, uint32_t transfer_id);
int tile_request_resource(tile_interrupt_t* tile_irq, uint32_t resource_type);
int tile_signal_shutdown(tile_interrupt_t* tile_irq);

// Default tile ISR handlers for incoming IRQs
int default_tile_shutdown_isr(interrupt_request_t* irq);
int default_tile_resource_grant_isr(interrupt_request_t* irq);
int default_tile_config_update_isr(interrupt_request_t* irq);
int default_tile_generic_isr(interrupt_request_t* irq);

#endif // TILE_INTERRUPT_H 