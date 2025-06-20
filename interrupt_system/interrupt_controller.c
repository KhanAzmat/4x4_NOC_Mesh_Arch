#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "interrupt_controller.h"

// Initialize C0 master interrupt controller
int interrupt_controller_init(interrupt_controller_t* ctrl) {
    if (!ctrl) {
        printf("ERROR: interrupt_controller_init called with NULL pointer\n");
        return -1;
    }
    
    // Initialize queue
    memset(ctrl->irq_queue, 0, sizeof(ctrl->irq_queue));
    ctrl->queue_head = 0;
    ctrl->queue_tail = 0;
    ctrl->queue_count = 0;
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&ctrl->queue_lock, NULL) != 0) {
        printf("ERROR: Failed to initialize queue mutex\n");
        return -1;
    }
    
    if (pthread_cond_init(&ctrl->irq_available, NULL) != 0) {
        printf("ERROR: Failed to initialize condition variable\n");
        pthread_mutex_destroy(&ctrl->queue_lock);
        return -1;
    }
    
    // Initialize IRQ masking - enable all by default
    for (int i = 0; i < MAX_TILES; i++) {
        ctrl->tile_irq_enabled[i] = true;
    }
    
    for (int i = 0; i <= IRQ_TYPE_MAX; i++) {
        ctrl->type_irq_enabled[i] = true;
    }
    
    ctrl->controller_enabled = true;
    
    // Initialize ISR handlers to NULL (will be set by register function)
    memset(ctrl->isr_handlers, 0, sizeof(ctrl->isr_handlers));
    
    // Initialize statistics
    memset(ctrl->irqs_received, 0, sizeof(ctrl->irqs_received));
    memset(ctrl->irqs_processed, 0, sizeof(ctrl->irqs_processed));
    ctrl->irqs_dropped = 0;
    ctrl->irqs_masked = 0;
    ctrl->total_processing_time_ns = 0;
    
    // Initialize processing thread
    ctrl->processor_running = false;
    
    printf("INFO: Interrupt controller initialized successfully\n");
    return 0;
}

// Destroy interrupt controller
int interrupt_controller_destroy(interrupt_controller_t* ctrl) {
    if (!ctrl) {
        return -1;
    }
    
    // Stop processing thread if running
    if (ctrl->processor_running) {
        ctrl->processor_running = false;
        pthread_cond_signal(&ctrl->irq_available);
        pthread_join(ctrl->irq_processor_thread, NULL);
    }
    
    // Destroy mutex and condition variable
    pthread_mutex_destroy(&ctrl->queue_lock);
    pthread_cond_destroy(&ctrl->irq_available);
    
    printf("INFO: Interrupt controller destroyed\n");
    return 0;
}

// Receive IRQ from tile and add to queue
int interrupt_receive_from_tile(interrupt_controller_t* ctrl, interrupt_request_t* irq) {
    if (!ctrl || !irq) {
        return -1;
    }
    
    // Check if controller is enabled
    if (!ctrl->controller_enabled) {
        ctrl->irqs_masked++;
        return -2; // Controller disabled
    }
    
    // Check tile masking
    if (irq->source_tile >= 0 && irq->source_tile < MAX_TILES && 
        !ctrl->tile_irq_enabled[irq->source_tile]) {
        ctrl->irqs_masked++;
        return -3; // Tile masked
    }
    
    // Check type masking
    if (irq->type >= 0 && irq->type <= IRQ_TYPE_MAX && 
        !ctrl->type_irq_enabled[irq->type]) {
        ctrl->irqs_masked++;
        return -4; // Type masked
    }
    
    pthread_mutex_lock(&ctrl->queue_lock);
    
    // Check if queue is full
    if (ctrl->queue_count >= MAX_PENDING_IRQS) {
        ctrl->irqs_dropped++;
        pthread_mutex_unlock(&ctrl->queue_lock);
        printf("WARNING: IRQ queue full, dropping IRQ from tile %d type %s\n", 
               irq->source_tile, get_irq_type_name(irq->type));
        return -5; // Queue full
    }
    
    // Add IRQ to queue
    ctrl->irq_queue[ctrl->queue_tail] = *irq;
    ctrl->irq_queue[ctrl->queue_tail].valid = true;
    ctrl->queue_tail = (ctrl->queue_tail + 1) % MAX_PENDING_IRQS;
    ctrl->queue_count++;
    
    // Update statistics
    if (irq->source_tile >= 0 && irq->source_tile < MAX_TILES) {
        ctrl->irqs_received[irq->source_tile]++;
    }
    
    // Signal that IRQ is available
    pthread_cond_signal(&ctrl->irq_available);
    pthread_mutex_unlock(&ctrl->queue_lock);
    
    printf("DEBUG: Received IRQ from tile %d: %s (queue: %d/%d)\n", 
           irq->source_tile, get_irq_type_name(irq->type), 
           ctrl->queue_count, MAX_PENDING_IRQS);
    
    return 0;
}

// Send IRQ to tile (placeholder for now)
int interrupt_send_to_tile(interrupt_controller_t* ctrl, int target_tile, 
                          interrupt_type_t type, uint32_t data, const char* message) {
    if (!ctrl) {
        return -1;
    }
    
    if (target_tile < 1 || target_tile >= MAX_TILES) {
        printf("ERROR: Invalid target tile %d\n", target_tile);
        return -2;
    }
    
    // TODO: Implement actual communication to tiles
    // For now, just log the action
    printf("INFO: Sending IRQ to tile %d: type=%s, data=0x%x, message='%s'\n", 
           target_tile, get_irq_type_name(type), data, message ? message : "");
    
    return 0;
}

// Process IRQ queue (handles one IRQ)
int interrupt_process_queue(interrupt_controller_t* ctrl) {
    if (!ctrl) {
        return -1;
    }
    
    pthread_mutex_lock(&ctrl->queue_lock);
    
    // Check if queue is empty
    if (ctrl->queue_count == 0) {
        pthread_mutex_unlock(&ctrl->queue_lock);
        return 0; // No IRQs to process
    }
    
    // Get next IRQ from queue
    interrupt_request_t irq = ctrl->irq_queue[ctrl->queue_head];
    ctrl->queue_head = (ctrl->queue_head + 1) % MAX_PENDING_IRQS;
    ctrl->queue_count--;
    
    pthread_mutex_unlock(&ctrl->queue_lock);
    
    // Process IRQ outside of lock
    if (!irq.valid) {
        printf("WARNING: Processing invalid IRQ\n");
        return -2;
    }
    
    uint64_t start_time = get_current_timestamp_ns();
    int result = 0;
    
    // Call appropriate ISR handler
    if (irq.type >= 0 && irq.type <= IRQ_TYPE_MAX && ctrl->isr_handlers[irq.type]) {
        result = ctrl->isr_handlers[irq.type](&irq);
    } else {
        // Use default handler
        result = default_generic_isr(&irq);
    }
    
    uint64_t end_time = get_current_timestamp_ns();
    ctrl->total_processing_time_ns += (end_time - start_time);
    
    // Update statistics
    if (irq.type >= 0 && irq.type <= IRQ_TYPE_MAX) {
        ctrl->irqs_processed[irq.type]++;
    }
    
    printf("DEBUG: Processed IRQ from tile %d: %s (result=%d, time=%lu ns)\n", 
           irq.source_tile, get_irq_type_name(irq.type), result, 
           (end_time - start_time));
    
    return 1; // Processed one IRQ
}

// IRQ processor thread main function
void* interrupt_processor_thread_main(void* arg) {
    interrupt_controller_t* ctrl = (interrupt_controller_t*)arg;
    
    if (!ctrl) {
        printf("ERROR: IRQ processor thread started with NULL controller\n");
        return NULL;
    }
    
    printf("INFO: IRQ processor thread started\n");
    
    while (ctrl->processor_running) {
        pthread_mutex_lock(&ctrl->queue_lock);
        
        // Wait for IRQs to be available
        while (ctrl->queue_count == 0 && ctrl->processor_running) {
            pthread_cond_wait(&ctrl->irq_available, &ctrl->queue_lock);
        }
        
        pthread_mutex_unlock(&ctrl->queue_lock);
        
        // Process all available IRQs
        while (ctrl->processor_running && interrupt_process_queue(ctrl) > 0) {
            // Continue processing
        }
    }
    
    printf("INFO: IRQ processor thread stopped\n");
    return NULL;
}

// Register ISR handler
void interrupt_register_isr(interrupt_controller_t* ctrl, interrupt_type_t type, interrupt_isr_t handler) {
    if (!ctrl || type < 0 || type > IRQ_TYPE_MAX) {
        return;
    }
    
    ctrl->isr_handlers[type] = handler;
    printf("INFO: Registered ISR for interrupt type %s\n", get_irq_type_name(type));
}

// Unregister ISR handler
void interrupt_unregister_isr(interrupt_controller_t* ctrl, interrupt_type_t type) {
    if (!ctrl || type < 0 || type > IRQ_TYPE_MAX) {
        return;
    }
    
    ctrl->isr_handlers[type] = NULL;
    printf("INFO: Unregistered ISR for interrupt type %s\n", get_irq_type_name(type));
}

// Control functions
int interrupt_enable_controller(interrupt_controller_t* ctrl) {
    if (!ctrl) return -1;
    ctrl->controller_enabled = true;
    printf("INFO: Interrupt controller enabled\n");
    return 0;
}

int interrupt_disable_controller(interrupt_controller_t* ctrl) {
    if (!ctrl) return -1;
    ctrl->controller_enabled = false;
    printf("INFO: Interrupt controller disabled\n");
    return 0;
}

int interrupt_enable_tile(interrupt_controller_t* ctrl, int tile_id) {
    if (!ctrl || tile_id < 0 || tile_id >= MAX_TILES) return -1;
    ctrl->tile_irq_enabled[tile_id] = true;
    printf("INFO: Enabled interrupts from tile %d\n", tile_id);
    return 0;
}

int interrupt_disable_tile(interrupt_controller_t* ctrl, int tile_id) {
    if (!ctrl || tile_id < 0 || tile_id >= MAX_TILES) return -1;
    ctrl->tile_irq_enabled[tile_id] = false;
    printf("INFO: Disabled interrupts from tile %d\n", tile_id);
    return 0;
}

int interrupt_enable_type(interrupt_controller_t* ctrl, interrupt_type_t type) {
    if (!ctrl || type < 0 || type > IRQ_TYPE_MAX) return -1;
    ctrl->type_irq_enabled[type] = true;
    printf("INFO: Enabled interrupt type %s\n", get_irq_type_name(type));
    return 0;
}

int interrupt_disable_type(interrupt_controller_t* ctrl, interrupt_type_t type) {
    if (!ctrl || type < 0 || type > IRQ_TYPE_MAX) return -1;
    ctrl->type_irq_enabled[type] = false;
    printf("INFO: Disabled interrupt type %s\n", get_irq_type_name(type));
    return 0;
}

// Utility functions
int interrupt_queue_space_available(interrupt_controller_t* ctrl) {
    if (!ctrl) return -1;
    
    pthread_mutex_lock(&ctrl->queue_lock);
    int space = MAX_PENDING_IRQS - ctrl->queue_count;
    pthread_mutex_unlock(&ctrl->queue_lock);
    
    return space;
}

int interrupt_queue_count(interrupt_controller_t* ctrl) {
    if (!ctrl) return -1;
    
    pthread_mutex_lock(&ctrl->queue_lock);
    int count = ctrl->queue_count;
    pthread_mutex_unlock(&ctrl->queue_lock);
    
    return count;
}

void interrupt_print_statistics(interrupt_controller_t* ctrl) {
    if (!ctrl) return;
    
    printf("\n=== Interrupt Controller Statistics ===\n");
    printf("Controller enabled: %s\n", ctrl->controller_enabled ? "Yes" : "No");
    printf("Queue: %d/%d IRQs\n", interrupt_queue_count(ctrl), MAX_PENDING_IRQS);
    printf("IRQs dropped: %lu\n", ctrl->irqs_dropped);
    printf("IRQs masked: %lu\n", ctrl->irqs_masked);
    printf("Total processing time: %lu ns\n", ctrl->total_processing_time_ns);
    
    printf("\nIRQs received by tile:\n");
    for (int i = 0; i < MAX_TILES; i++) {
        if (ctrl->irqs_received[i] > 0) {
            printf("  Tile %d: %lu IRQs\n", i, ctrl->irqs_received[i]);
        }
    }
    
    printf("\nIRQs processed by type:\n");
    for (int i = 1; i <= IRQ_TYPE_MAX; i++) {
        if (ctrl->irqs_processed[i] > 0) {
            printf("  %s: %lu IRQs\n", get_irq_type_name(i), ctrl->irqs_processed[i]);
        }
    }
    printf("=====================================\n\n");
}

void interrupt_reset_statistics(interrupt_controller_t* ctrl) {
    if (!ctrl) return;
    
    memset(ctrl->irqs_received, 0, sizeof(ctrl->irqs_received));
    memset(ctrl->irqs_processed, 0, sizeof(ctrl->irqs_processed));
    ctrl->irqs_dropped = 0;
    ctrl->irqs_masked = 0;
    ctrl->total_processing_time_ns = 0;
    
    printf("INFO: Interrupt statistics reset\n");
}

// Default ISR handlers
int default_task_complete_isr(interrupt_request_t* irq) {
    printf("ISR: Task %u completed on tile %d\n", irq->data, irq->source_tile);
    return 0;
}

int default_error_isr(interrupt_request_t* irq) {
    printf("ISR: ERROR on tile %d - code 0x%x: %s\n", 
           irq->source_tile, irq->data, irq->message);
    return 0;
}

int default_dma_complete_isr(interrupt_request_t* irq) {
    printf("ISR: DMA transfer %u completed on tile %d\n", irq->data, irq->source_tile);
    return 0;
}

int default_resource_request_isr(interrupt_request_t* irq) {
    printf("ISR: Resource request (type %u) from tile %d\n", irq->data, irq->source_tile);
    return 0;
}

int default_shutdown_isr(interrupt_request_t* irq) {
    printf("ISR: Shutdown request from tile %d: %s\n", irq->source_tile, irq->message);
    return 0;
}

int default_generic_isr(interrupt_request_t* irq) {
    printf("ISR: Generic IRQ from tile %d - type %s, data 0x%x: %s\n", 
           irq->source_tile, get_irq_type_name(irq->type), irq->data, irq->message);
    return 0;
} 