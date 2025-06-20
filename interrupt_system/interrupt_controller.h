#ifndef INTERRUPT_CONTROLLER_H
#define INTERRUPT_CONTROLLER_H

#include <pthread.h>
#include "interrupt_types.h"

// Forward declaration for ISR function pointer type
typedef int (*interrupt_isr_t)(interrupt_request_t* irq);

// C0 Master Interrupt Controller - manages all incoming IRQs from tiles
typedef struct {
    // IRQ queue for incoming interrupts (circular buffer)
    interrupt_request_t irq_queue[MAX_PENDING_IRQS];
    int queue_head;                    // Next position to read from
    int queue_tail;                    // Next position to write to
    int queue_count;                   // Current number of IRQs in queue
    pthread_mutex_t queue_lock;        // Protects queue operations
    pthread_cond_t irq_available;      // Signals when IRQ is available
    
    // IRQ masking and control
    bool tile_irq_enabled[MAX_TILES];  // Enable/disable IRQs per tile
    bool type_irq_enabled[IRQ_TYPE_MAX + 1]; // Enable/disable IRQs per type
    bool controller_enabled;           // Master enable/disable
    
    // Interrupt Service Routine handlers
    interrupt_isr_t isr_handlers[IRQ_TYPE_MAX + 1];
    
    // Statistics and monitoring
    uint64_t irqs_received[MAX_TILES]; // Total IRQs received per tile
    uint64_t irqs_processed[IRQ_TYPE_MAX + 1]; // Total IRQs processed per type
    uint64_t irqs_dropped;             // IRQs dropped due to queue full
    uint64_t irqs_masked;              // IRQs dropped due to masking
    uint64_t total_processing_time_ns; // Total time spent in ISRs
    
    // Processing thread
    pthread_t irq_processor_thread;
    bool processor_running;
    
} interrupt_controller_t;

// C0 Master interrupt controller functions
int interrupt_controller_init(interrupt_controller_t* ctrl);
int interrupt_controller_destroy(interrupt_controller_t* ctrl);

// IRQ reception and transmission
int interrupt_receive_from_tile(interrupt_controller_t* ctrl, interrupt_request_t* irq);
int interrupt_send_to_tile(interrupt_controller_t* ctrl, int target_tile, interrupt_type_t type, uint32_t data, const char* message);

// IRQ processing
int interrupt_process_queue(interrupt_controller_t* ctrl);
void* interrupt_processor_thread_main(void* arg);

// ISR management
void interrupt_register_isr(interrupt_controller_t* ctrl, interrupt_type_t type, interrupt_isr_t handler);
void interrupt_unregister_isr(interrupt_controller_t* ctrl, interrupt_type_t type);

// Control and configuration
int interrupt_enable_controller(interrupt_controller_t* ctrl);
int interrupt_disable_controller(interrupt_controller_t* ctrl);
int interrupt_enable_tile(interrupt_controller_t* ctrl, int tile_id);
int interrupt_disable_tile(interrupt_controller_t* ctrl, int tile_id);
int interrupt_enable_type(interrupt_controller_t* ctrl, interrupt_type_t type);
int interrupt_disable_type(interrupt_controller_t* ctrl, interrupt_type_t type);

// Utility and monitoring
int interrupt_queue_space_available(interrupt_controller_t* ctrl);
int interrupt_queue_count(interrupt_controller_t* ctrl);
void interrupt_print_statistics(interrupt_controller_t* ctrl);
void interrupt_reset_statistics(interrupt_controller_t* ctrl);

// Default ISR handlers
int default_task_complete_isr(interrupt_request_t* irq);
int default_error_isr(interrupt_request_t* irq);
int default_dma_complete_isr(interrupt_request_t* irq);
int default_resource_request_isr(interrupt_request_t* irq);
int default_shutdown_isr(interrupt_request_t* irq);
int default_generic_isr(interrupt_request_t* irq);

#endif // INTERRUPT_CONTROLLER_H 