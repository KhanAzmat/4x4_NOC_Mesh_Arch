#ifndef C0_CONTROLLER_H
#define C0_CONTROLLER_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// ============================================================================
// INTERRUPT SYSTEM INTEGRATION
// ============================================================================

// Interrupt types - integrated into platform
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

// Interrupt priority levels
typedef enum {
    IRQ_PRIORITY_CRITICAL = 0,     // Errors, shutdown
    IRQ_PRIORITY_HIGH = 1,         // DMA complete, resource requests  
    IRQ_PRIORITY_NORMAL = 2,       // Task complete
    IRQ_PRIORITY_LOW = 3           // Congestion, timers
} interrupt_priority_t;

// Interrupt request structure
typedef struct {
    int source_tile;               // Which tile sent the IRQ (1-7, or 0 for C0)
    interrupt_type_t type;         // Type of interrupt
    interrupt_priority_t priority; // Priority level
    uint64_t timestamp;            // When IRQ was generated (nanoseconds)
    uint32_t data;                 // Additional IRQ data (task_id, error_code, etc.)
    char message[64];              // Optional descriptive message
    bool valid;                    // Flag to indicate if IRQ is valid
} interrupt_request_t;

// Compact interrupt packet for NoC transmission (fits in 64-byte payload)
typedef struct __attribute__((packed)) {
    uint32_t source_tile;          // 4 bytes
    uint32_t type;                 // 4 bytes 
    uint32_t priority;             // 4 bytes
    uint64_t timestamp;            // 8 bytes
    uint32_t data;                 // 4 bytes
    uint32_t valid;                // 4 bytes 
    char message[36];              // 36 bytes = 64 bytes total
} compact_interrupt_packet_t;

// Interrupt handler function pointer
typedef int (*interrupt_handler_t)(interrupt_request_t* irq, void* platform_context);

// C0 Master interrupt controller (use NUM_TILES for consistency)
#define MAX_PENDING_IRQS 64
typedef struct {
    interrupt_request_t irq_queue[MAX_PENDING_IRQS];
    int head, tail, count;
    pthread_mutex_t irq_lock;
    pthread_cond_t irq_available;
    
    // ISR handlers for different interrupt types
    interrupt_handler_t isr_handlers[IRQ_TYPE_MAX + 1];
    
    // Statistics
    uint64_t interrupts_received;
    uint64_t interrupts_processed;
    uint64_t interrupts_dropped;
    
    // Processing control
    volatile bool processing_enabled;
} c0_interrupt_controller_t;

// Helper functions for interrupts
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

static inline uint64_t get_current_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// ============================================================================
// TASK SYSTEM DEFINITIONS
// ============================================================================

// STEP 2: Task system definitions
typedef enum {
    TASK_TYPE_MEMORY_COPY,
    TASK_TYPE_DMA_TRANSFER, 
    TASK_TYPE_COMPUTATION,
    TASK_TYPE_NOC_TRANSFER,
    TASK_TYPE_TEST_EXECUTION,
    TASK_TYPE_HAL_TEST      // New: For HAL test execution
} task_type_t;

typedef struct {
    int task_id;
    task_type_t type;
    int assigned_tile;
    volatile bool completed;
    volatile bool taken;  // Flag to prevent double execution
    int result;
    
    // Task parameters (union for different task types)
    union {
        struct {
            uint64_t src_addr;
            uint64_t dst_addr;
            size_t size;
        } memory_op;
        
        struct {
            int test_id;
            void* test_data;
        } test_exec;
        
        struct {
            int (*test_func)(void*);  // HAL test function pointer
            const char* test_name;    // Test name for logging
            void* platform;           // Platform context
            int* result_ptr;          // Pointer to store result
        } hal_test;
    } params;
    
    // Synchronization
    pthread_mutex_t task_lock;
    pthread_cond_t task_complete;
} task_t;

// STEP 2: Task queue system
#define MAX_PENDING_TASKS 64

typedef struct {
    task_t tasks[MAX_PENDING_TASKS];
    int head, tail, count;
    pthread_mutex_t queue_lock;
    pthread_cond_t task_available;
    pthread_cond_t queue_not_full;
} task_queue_t;

// STEP 2: Enhanced tile_core_t with task execution infrastructure
typedef struct {
    int id;
    int x, y;
    
    // Address space (what software sees)
    uint64_t dlm64_base_addr;
    uint64_t dlm1_512_base_addr;
    uint64_t dma_reg_base_addr;
    
    // Simulated hardware memory (what tests access for verification)
    uint8_t* dlm64_ptr;
    uint8_t* dlm1_512_ptr;
    uint8_t* dma_regs_ptr;
    
    // STEP 1: Basic threading infrastructure
    pthread_t thread_id;
    volatile bool running;
    volatile bool initialized;
    pthread_mutex_t state_lock;
    
    // STEP 2: Task execution infrastructure
    task_t* current_task;
    volatile bool task_pending;
    volatile bool idle;
    
    // Performance counters
    int tasks_completed;
    uint64_t total_execution_time;
    
    // NEW: Interrupt capabilities for tiles
    uint64_t interrupts_sent;
    uint64_t last_interrupt_timestamp;
    
} tile_core_t;

typedef struct {
    int id;
    uint64_t dmem_base_addr;    // Address space
    uint8_t* dmem_ptr;          // Simulated memory
    size_t dmem_size;
} dmem_module_t;

// STEP 2: Enhanced platform with task coordination system
typedef struct {
    tile_core_t* nodes;
    int node_count;
    dmem_module_t* dmems;
    int dmem_count;
    
    uint8_t* memory_pool;
    size_t total_memory_size;
    
    // STEP 1: Platform control (main thread acts as C0 master)
    volatile bool platform_running;
    pthread_mutex_t platform_lock;
    
    // STEP 2: Task coordination system
    task_queue_t task_queue;
    int next_task_id;
    pthread_mutex_t task_id_lock;
    
    // C0 master coordination
    volatile int active_tasks;
    volatile int completed_tasks;
    
    // NEW: Integrated interrupt system
    c0_interrupt_controller_t interrupt_controller;
    volatile bool interrupt_processing_enabled;
    
} mesh_platform_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// STEP 1: Thread management function declarations
int platform_start_tile_threads(mesh_platform_t* p);
int platform_stop_tile_threads(mesh_platform_t* p);
void* tile_processor_main(void* arg);

// C0 master functions (executed by main thread)
void c0_master_supervise_tiles(mesh_platform_t* p);
void c0_run_test_suite(mesh_platform_t* platform);

// STEP 2: Task system function declarations
int task_queue_init(task_queue_t* queue);
int task_queue_destroy(task_queue_t* queue);
int task_queue_push(task_queue_t* queue, task_t* task);
task_t* task_queue_pop(task_queue_t* queue);
bool task_queue_is_empty(task_queue_t* queue);

// STEP 2: C0 master task coordination functions
task_t* c0_create_task(mesh_platform_t* p, task_type_t type, int target_tile);
int c0_queue_task(mesh_platform_t* p, task_t* task);
int c0_wait_for_completion(mesh_platform_t* p, int expected_tasks);

// STEP 2: Tile task execution functions
task_t* tile_get_next_task(mesh_platform_t* p, tile_core_t* tile);
int tile_execute_task(tile_core_t* tile, task_t* task);
int tile_complete_task(mesh_platform_t* p, tile_core_t* tile, task_t* task);

// STEP 2: HAL test task management functions
task_t* create_hal_test_task(mesh_platform_t* p, 
                            int (*test_func)(void*),
                            const char* test_name,
                            int* result_ptr);
int queue_task_to_available_tile(mesh_platform_t* p, task_t* task);
int wait_for_all_tasks_completion(mesh_platform_t* p, int expected_count);
void c0_run_hal_tests_distributed(mesh_platform_t* platform);

// ============================================================================
// NEW: INTERRUPT SYSTEM FUNCTION DECLARATIONS
// ============================================================================

// C0 Master interrupt controller functions
int c0_interrupt_controller_init(c0_interrupt_controller_t* ctrl);
int c0_interrupt_controller_destroy(c0_interrupt_controller_t* ctrl);
int c0_process_pending_interrupts(mesh_platform_t* p);
int c0_register_interrupt_handler(mesh_platform_t* p, interrupt_type_t type, interrupt_handler_t handler);

// Tile interrupt functions
int tile_send_interrupt_to_c0(mesh_platform_t* p, int tile_id, interrupt_type_t type, 
                              uint32_t data, const char* message);
int tile_signal_task_complete(mesh_platform_t* p, int tile_id, uint32_t task_id);
int tile_signal_error(mesh_platform_t* p, int tile_id, uint32_t error_code, const char* error_msg);
int tile_signal_dma_complete(mesh_platform_t* p, int tile_id, uint32_t transfer_id);

// NoC interrupt packet functions
int noc_send_interrupt_packet(int src_tile, int dst_tile, interrupt_request_t* irq);
int noc_handle_received_interrupt(mesh_platform_t* p, interrupt_request_t* irq);

// Default interrupt handlers
int default_task_complete_handler(interrupt_request_t* irq, void* platform_context);
int default_error_handler(interrupt_request_t* irq, void* platform_context);
int default_dma_complete_handler(interrupt_request_t* irq, void* platform_context);
int default_resource_request_handler(interrupt_request_t* irq, void* platform_context);
int default_shutdown_handler(interrupt_request_t* irq, void* platform_context);

#endif