#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "tile_interrupt.h"

// Initialize tile interrupt interface
int tile_interrupt_init(tile_interrupt_t* tile_irq, int tile_id) {
    if (!tile_irq) {
        printf("ERROR: tile_interrupt_init called with NULL pointer\n");
        return -1;
    }
    
    if (tile_id < 1 || tile_id >= MAX_TILES) {
        printf("ERROR: Invalid tile ID %d (must be 1-7)\n", tile_id);
        return -2;
    }
    
    tile_irq->tile_id = tile_id;
    
    // Initialize incoming queue
    memset(tile_irq->incoming_queue, 0, sizeof(tile_irq->incoming_queue));
    tile_irq->incoming_head = 0;
    tile_irq->incoming_tail = 0;
    tile_irq->incoming_count = 0;
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&tile_irq->incoming_lock, NULL) != 0) {
        printf("ERROR: Failed to initialize incoming queue mutex for tile %d\n", tile_id);
        return -3;
    }
    
    if (pthread_cond_init(&tile_irq->incoming_available, NULL) != 0) {
        printf("ERROR: Failed to initialize condition variable for tile %d\n", tile_id);
        pthread_mutex_destroy(&tile_irq->incoming_lock);
        return -4;
    }
    
    // Initialize IRQ masking - enable all by default
    for (int i = 0; i <= IRQ_TYPE_MAX; i++) {
        tile_irq->incoming_irq_enabled[i] = true;
    }
    
    tile_irq->interrupt_enabled = true;
    
    // Initialize ISR handlers to NULL
    memset(tile_irq->incoming_isr_handlers, 0, sizeof(tile_irq->incoming_isr_handlers));
    
    // Initialize statistics
    tile_irq->irqs_sent_to_c0 = 0;
    tile_irq->irqs_received_from_c0 = 0;
    tile_irq->irqs_dropped_incoming = 0;
    tile_irq->irqs_masked_incoming = 0;
    tile_irq->send_failures = 0;
    
    // Initialize processing thread
    tile_irq->incoming_processor_running = false;
    
    printf("INFO: Tile %d interrupt interface initialized successfully\n", tile_id);
    return 0;
}

// Destroy tile interrupt interface
int tile_interrupt_destroy(tile_interrupt_t* tile_irq) {
    if (!tile_irq) {
        return -1;
    }
    
    // Stop processing thread if running
    if (tile_irq->incoming_processor_running) {
        tile_irq->incoming_processor_running = false;
        pthread_cond_signal(&tile_irq->incoming_available);
        pthread_join(tile_irq->incoming_processor_thread, NULL);
    }
    
    // Destroy mutex and condition variable
    pthread_mutex_destroy(&tile_irq->incoming_lock);
    pthread_cond_destroy(&tile_irq->incoming_available);
    
    printf("INFO: Tile %d interrupt interface destroyed\n", tile_irq->tile_id);
    return 0;
}

// Send interrupt to C0 master
int tile_send_interrupt_to_c0(tile_interrupt_t* tile_irq, interrupt_type_t type, 
                              uint32_t data, const char* message) {
    if (!tile_irq) {
        return -1;
    }
    
    if (!tile_irq->interrupt_enabled) {
        return -2; // Interrupts disabled
    }
    
    // Create interrupt request
    interrupt_request_t irq;
    memset(&irq, 0, sizeof(irq));
    
    irq.source_tile = tile_irq->tile_id;
    irq.type = type;
    irq.priority = get_irq_priority(type);
    irq.timestamp = get_current_timestamp_ns();
    irq.data = data;
    irq.valid = true;
    
    if (message) {
        strncpy(irq.message, message, sizeof(irq.message) - 1);
        irq.message[sizeof(irq.message) - 1] = '\0';
    } else {
        irq.message[0] = '\0';
    }
    
    // TODO: Implement actual communication to C0
    // For now, just log the action and update statistics
    printf("INFO: Tile %d sending IRQ to C0: type=%s, data=0x%x, message='%s'\n", 
           tile_irq->tile_id, get_irq_type_name(type), data, message ? message : "");
    
    tile_irq->irqs_sent_to_c0++;
    
    return 0;
}

// Receive interrupt from C0 master
int tile_receive_interrupt_from_c0(tile_interrupt_t* tile_irq, interrupt_request_t* irq) {
    if (!tile_irq || !irq) {
        return -1;
    }
    
    // Check if interrupts are enabled
    if (!tile_irq->interrupt_enabled) {
        tile_irq->irqs_masked_incoming++;
        return -2; // Interrupts disabled
    }
    
    // Check type masking
    if (irq->type >= 0 && irq->type <= IRQ_TYPE_MAX && 
        !tile_irq->incoming_irq_enabled[irq->type]) {
        tile_irq->irqs_masked_incoming++;
        return -3; // Type masked
    }
    
    pthread_mutex_lock(&tile_irq->incoming_lock);
    
    // Check if queue is full
    if (tile_irq->incoming_count >= MAX_INCOMING_IRQS) {
        tile_irq->irqs_dropped_incoming++;
        pthread_mutex_unlock(&tile_irq->incoming_lock);
        printf("WARNING: Tile %d incoming IRQ queue full, dropping IRQ type %s\n", 
               tile_irq->tile_id, get_irq_type_name(irq->type));
        return -4; // Queue full
    }
    
    // Add IRQ to incoming queue
    tile_irq->incoming_queue[tile_irq->incoming_tail] = *irq;
    tile_irq->incoming_queue[tile_irq->incoming_tail].valid = true;
    tile_irq->incoming_tail = (tile_irq->incoming_tail + 1) % MAX_INCOMING_IRQS;
    tile_irq->incoming_count++;
    
    // Update statistics
    tile_irq->irqs_received_from_c0++;
    
    // Signal that IRQ is available
    pthread_cond_signal(&tile_irq->incoming_available);
    pthread_mutex_unlock(&tile_irq->incoming_lock);
    
    printf("DEBUG: Tile %d received IRQ from C0: %s (queue: %d/%d)\n", 
           tile_irq->tile_id, get_irq_type_name(irq->type), 
           tile_irq->incoming_count, MAX_INCOMING_IRQS);
    
    return 0;
}

// Process incoming IRQ queue (handles one IRQ)
int tile_process_incoming_queue(tile_interrupt_t* tile_irq) {
    if (!tile_irq) {
        return -1;
    }
    
    pthread_mutex_lock(&tile_irq->incoming_lock);
    
    // Check if queue is empty
    if (tile_irq->incoming_count == 0) {
        pthread_mutex_unlock(&tile_irq->incoming_lock);
        return 0; // No IRQs to process
    }
    
    // Get next IRQ from queue
    interrupt_request_t irq = tile_irq->incoming_queue[tile_irq->incoming_head];
    tile_irq->incoming_head = (tile_irq->incoming_head + 1) % MAX_INCOMING_IRQS;
    tile_irq->incoming_count--;
    
    pthread_mutex_unlock(&tile_irq->incoming_lock);
    
    // Process IRQ outside of lock
    if (!irq.valid) {
        printf("WARNING: Tile %d processing invalid IRQ\n", tile_irq->tile_id);
        return -2;
    }
    
    int result = 0;
    
    // Call appropriate ISR handler
    if (irq.type >= 0 && irq.type <= IRQ_TYPE_MAX && tile_irq->incoming_isr_handlers[irq.type]) {
        result = tile_irq->incoming_isr_handlers[irq.type](&irq);
    } else {
        // Use default handler
        result = default_tile_generic_isr(&irq);
    }
    
    printf("DEBUG: Tile %d processed incoming IRQ: %s (result=%d)\n", 
           tile_irq->tile_id, get_irq_type_name(irq.type), result);
    
    return 1; // Processed one IRQ
}

// Incoming IRQ processor thread main function
void* tile_incoming_processor_thread_main(void* arg) {
    tile_interrupt_t* tile_irq = (tile_interrupt_t*)arg;
    
    if (!tile_irq) {
        printf("ERROR: Tile incoming IRQ processor thread started with NULL interface\n");
        return NULL;
    }
    
    printf("INFO: Tile %d incoming IRQ processor thread started\n", tile_irq->tile_id);
    
    while (tile_irq->incoming_processor_running) {
        pthread_mutex_lock(&tile_irq->incoming_lock);
        
        // Wait for IRQs to be available
        while (tile_irq->incoming_count == 0 && tile_irq->incoming_processor_running) {
            pthread_cond_wait(&tile_irq->incoming_available, &tile_irq->incoming_lock);
        }
        
        pthread_mutex_unlock(&tile_irq->incoming_lock);
        
        // Process all available IRQs
        while (tile_irq->incoming_processor_running && tile_process_incoming_queue(tile_irq) > 0) {
            // Continue processing
        }
    }
    
    printf("INFO: Tile %d incoming IRQ processor thread stopped\n", tile_irq->tile_id);
    return NULL;
}

// Register ISR handler for incoming IRQs
void tile_register_incoming_isr(tile_interrupt_t* tile_irq, interrupt_type_t type, tile_interrupt_isr_t handler) {
    if (!tile_irq || type < 0 || type > IRQ_TYPE_MAX) {
        return;
    }
    
    tile_irq->incoming_isr_handlers[type] = handler;
    printf("INFO: Tile %d registered incoming ISR for interrupt type %s\n", 
           tile_irq->tile_id, get_irq_type_name(type));
}

// Unregister ISR handler for incoming IRQs
void tile_unregister_incoming_isr(tile_interrupt_t* tile_irq, interrupt_type_t type) {
    if (!tile_irq || type < 0 || type > IRQ_TYPE_MAX) {
        return;
    }
    
    tile_irq->incoming_isr_handlers[type] = NULL;
    printf("INFO: Tile %d unregistered incoming ISR for interrupt type %s\n", 
           tile_irq->tile_id, get_irq_type_name(type));
}

// Control functions
int tile_enable_interrupts(tile_interrupt_t* tile_irq) {
    if (!tile_irq) return -1;
    tile_irq->interrupt_enabled = true;
    printf("INFO: Tile %d interrupts enabled\n", tile_irq->tile_id);
    return 0;
}

int tile_disable_interrupts(tile_interrupt_t* tile_irq) {
    if (!tile_irq) return -1;
    tile_irq->interrupt_enabled = false;
    printf("INFO: Tile %d interrupts disabled\n", tile_irq->tile_id);
    return 0;
}

int tile_enable_incoming_type(tile_interrupt_t* tile_irq, interrupt_type_t type) {
    if (!tile_irq || type < 0 || type > IRQ_TYPE_MAX) return -1;
    tile_irq->incoming_irq_enabled[type] = true;
    printf("INFO: Tile %d enabled incoming interrupt type %s\n", 
           tile_irq->tile_id, get_irq_type_name(type));
    return 0;
}

int tile_disable_incoming_type(tile_interrupt_t* tile_irq, interrupt_type_t type) {
    if (!tile_irq || type < 0 || type > IRQ_TYPE_MAX) return -1;
    tile_irq->incoming_irq_enabled[type] = false;
    printf("INFO: Tile %d disabled incoming interrupt type %s\n", 
           tile_irq->tile_id, get_irq_type_name(type));
    return 0;
}

// Utility functions
int tile_incoming_queue_space_available(tile_interrupt_t* tile_irq) {
    if (!tile_irq) return -1;
    
    pthread_mutex_lock(&tile_irq->incoming_lock);
    int space = MAX_INCOMING_IRQS - tile_irq->incoming_count;
    pthread_mutex_unlock(&tile_irq->incoming_lock);
    
    return space;
}

int tile_incoming_queue_count(tile_interrupt_t* tile_irq) {
    if (!tile_irq) return -1;
    
    pthread_mutex_lock(&tile_irq->incoming_lock);
    int count = tile_irq->incoming_count;
    pthread_mutex_unlock(&tile_irq->incoming_lock);
    
    return count;
}

void tile_print_interrupt_statistics(tile_interrupt_t* tile_irq) {
    if (!tile_irq) return;
    
    printf("\n=== Tile %d Interrupt Statistics ===\n", tile_irq->tile_id);
    printf("Interrupts enabled: %s\n", tile_irq->interrupt_enabled ? "Yes" : "No");
    printf("Incoming queue: %d/%d IRQs\n", tile_incoming_queue_count(tile_irq), MAX_INCOMING_IRQS);
    printf("IRQs sent to C0: %lu\n", tile_irq->irqs_sent_to_c0);
    printf("IRQs received from C0: %lu\n", tile_irq->irqs_received_from_c0);
    printf("Incoming IRQs dropped: %lu\n", tile_irq->irqs_dropped_incoming);
    printf("Incoming IRQs masked: %lu\n", tile_irq->irqs_masked_incoming);
    printf("Send failures: %lu\n", tile_irq->send_failures);
    printf("=====================================\n\n");
}

void tile_reset_interrupt_statistics(tile_interrupt_t* tile_irq) {
    if (!tile_irq) return;
    
    tile_irq->irqs_sent_to_c0 = 0;
    tile_irq->irqs_received_from_c0 = 0;
    tile_irq->irqs_dropped_incoming = 0;
    tile_irq->irqs_masked_incoming = 0;
    tile_irq->send_failures = 0;
    
    printf("INFO: Tile %d interrupt statistics reset\n", tile_irq->tile_id);
}

// Convenience functions for common IRQ types
int tile_signal_task_complete(tile_interrupt_t* tile_irq, uint32_t task_id) {
    return tile_send_interrupt_to_c0(tile_irq, IRQ_TYPE_TASK_COMPLETE, task_id, "Task completed");
}

int tile_signal_error(tile_interrupt_t* tile_irq, uint32_t error_code, const char* error_msg) {
    return tile_send_interrupt_to_c0(tile_irq, IRQ_TYPE_ERROR, error_code, error_msg);
}

int tile_signal_dma_complete(tile_interrupt_t* tile_irq, uint32_t transfer_id) {
    return tile_send_interrupt_to_c0(tile_irq, IRQ_TYPE_DMA_COMPLETE, transfer_id, "DMA transfer completed");
}

int tile_request_resource(tile_interrupt_t* tile_irq, uint32_t resource_type) {
    return tile_send_interrupt_to_c0(tile_irq, IRQ_TYPE_RESOURCE_REQUEST, resource_type, "Resource request");
}

int tile_signal_shutdown(tile_interrupt_t* tile_irq) {
    return tile_send_interrupt_to_c0(tile_irq, IRQ_TYPE_SHUTDOWN, 0, "Tile shutdown request");
}

// Default tile ISR handlers for incoming IRQs
int default_tile_shutdown_isr(interrupt_request_t* irq) {
    printf("TILE ISR: Shutdown command received: %s\n", irq->message);
    return 0;
}

int default_tile_resource_grant_isr(interrupt_request_t* irq) {
    printf("TILE ISR: Resource %u granted\n", irq->data);
    return 0;
}

int default_tile_config_update_isr(interrupt_request_t* irq) {
    printf("TILE ISR: Configuration update: %s\n", irq->message);
    return 0;
}

int default_tile_generic_isr(interrupt_request_t* irq) {
    printf("TILE ISR: Generic IRQ - type %s, data 0x%x: %s\n", 
           get_irq_type_name(irq->type), irq->data, irq->message);
    return 0;
} 