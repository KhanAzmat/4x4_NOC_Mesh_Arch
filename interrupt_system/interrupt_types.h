#ifndef INTERRUPT_TYPES_H
#define INTERRUPT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// System constants
#define MAX_TILES 8
#define MAX_PENDING_IRQS 64
#define MAX_INCOMING_IRQS 16

// Interrupt types - each represents different events that can trigger IRQs
typedef enum {
    IRQ_TYPE_TASK_COMPLETE = 1,    // Tile finished task
    IRQ_TYPE_ERROR = 2,            // Tile encountered error  
    IRQ_TYPE_DMA_COMPLETE = 3,     // DMA transfer finished
    IRQ_TYPE_NOC_CONGESTION = 4,   // NoC traffic congestion
    IRQ_TYPE_RESOURCE_REQUEST = 5,  // Tile needs resource
    IRQ_TYPE_CUSTOM = 6,           // Custom application IRQ
    IRQ_TYPE_TIMER = 7,            // Timer expiration
    IRQ_TYPE_SHUTDOWN = 8,         // Tile requesting shutdown
    IRQ_TYPE_MAX = 8
} interrupt_type_t;

// Interrupt priority levels - determines processing order
typedef enum {
    IRQ_PRIORITY_CRITICAL = 0,     // Errors, shutdown
    IRQ_PRIORITY_HIGH = 1,         // DMA complete, resource requests  
    IRQ_PRIORITY_NORMAL = 2,       // Task complete
    IRQ_PRIORITY_LOW = 3           // Congestion, timers
} interrupt_priority_t;

// Interrupt request structure - carries all IRQ information
typedef struct {
    int source_tile;               // Which tile sent the IRQ (1-7, or 0 for C0)
    interrupt_type_t type;         // Type of interrupt
    interrupt_priority_t priority; // Priority level
    uint64_t timestamp;            // When IRQ was generated (nanoseconds)
    uint32_t data;                 // Additional IRQ data (task_id, error_code, etc.)
    char message[64];              // Optional descriptive message
    bool valid;                    // Flag to indicate if IRQ is valid
} interrupt_request_t;

// Helper function to get priority for interrupt type
static inline interrupt_priority_t get_irq_priority(interrupt_type_t type) {
    switch (type) {
        case IRQ_TYPE_ERROR:
        case IRQ_TYPE_SHUTDOWN:
            return IRQ_PRIORITY_CRITICAL;
        case IRQ_TYPE_DMA_COMPLETE:
        case IRQ_TYPE_RESOURCE_REQUEST:
            return IRQ_PRIORITY_HIGH;
        case IRQ_TYPE_TASK_COMPLETE:
        case IRQ_TYPE_CUSTOM:
            return IRQ_PRIORITY_NORMAL;
        case IRQ_TYPE_NOC_CONGESTION:
        case IRQ_TYPE_TIMER:
            return IRQ_PRIORITY_LOW;
        default:
            return IRQ_PRIORITY_NORMAL;
    }
}

// Helper function to get IRQ type name
static inline const char* get_irq_type_name(interrupt_type_t type) {
    switch (type) {
        case IRQ_TYPE_TASK_COMPLETE: return "TASK_COMPLETE";
        case IRQ_TYPE_ERROR: return "ERROR";
        case IRQ_TYPE_DMA_COMPLETE: return "DMA_COMPLETE";
        case IRQ_TYPE_NOC_CONGESTION: return "NOC_CONGESTION";
        case IRQ_TYPE_RESOURCE_REQUEST: return "RESOURCE_REQUEST";
        case IRQ_TYPE_CUSTOM: return "CUSTOM";
        case IRQ_TYPE_TIMER: return "TIMER";
        case IRQ_TYPE_SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

// Helper function to get current timestamp in nanoseconds
static inline uint64_t get_current_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#endif // INTERRUPT_TYPES_H 