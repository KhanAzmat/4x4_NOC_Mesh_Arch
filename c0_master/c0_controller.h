#ifndef C0_CONTROLLER_H
#define C0_CONTROLLER_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// HAL/Driver includes for DMAC512 and PLIC integration
#include "hal_dmac512.h"
#include "plic.h"

// ============================================================================
// PLIC INTERRUPT SYSTEM INTEGRATION
// ============================================================================

// Platform uses PLIC HAL's irq_source_id_t directly
// Map platform concepts to PLIC interrupt sources:
// - Task completion → IRQ_MESH_NODE
// - DMA completion → IRQ_DMA512  
// - Errors → IRQ_GPIO (general purpose)
// - Timers → IRQ_PIT (periodic interrupt timer)
// - Other platform events → other PLIC sources

// Simple interrupt statistics for monitoring (optional)
typedef struct {
    uint64_t interrupts_claimed[IRQ_FX3 + 1];     // Per IRQ source claim count
    uint64_t interrupts_completed[IRQ_FX3 + 1];   // Per IRQ source completion count
    uint64_t hart_interrupts[8];                  // Per hart interrupt count (8 tiles)
} plic_interrupt_stats_t;

// Helper functions for PLIC integration
static inline uint8_t get_plic_priority(irq_source_id_t irq_id) {
    switch (irq_id) {
        case IRQ_GPIO:           // Errors - highest priority
            return 7;
        case IRQ_DMA512:         // DMA completion - high priority
        case IRQ_DMA:
            return 5;
        case IRQ_MESH_NODE:      // Task completion - normal priority
            return 3;
        case IRQ_PIT:            // Timer - low priority
            return 1;
        default:
            return 2;            // Default priority
    }
}

static inline const char* get_plic_irq_name(irq_source_id_t irq_id) {
    switch (irq_id) {
        case IRQ_WDT: return "WDT";
        case IRQ_RTC_PERIOD: return "RTC_PERIOD";
        case IRQ_RTC_ALARM: return "RTC_ALARM";
        case IRQ_PIT: return "PIT";
        case IRQ_SPI1: return "SPI1";
        case IRQ_SPI2: return "SPI2";
        case IRQ_I2C: return "I2C";
        case IRQ_GPIO: return "GPIO";
        case IRQ_UART1: return "UART1";
        case IRQ_USB_HOST: return "USB_HOST";
        case IRQ_DMA: return "DMA";
        case IRQ_DMA512: return "DMA512";
        case IRQ_MESH_NODE: return "MESH_NODE";
        case IRQ_FX3: return "FX3";
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
    
    // DMAC512 HAL integration
    DMAC512_RegDef* dmac512_regs;      // Points to DMAC512 registers in dma_regs_ptr
    DMAC512_HandleTypeDef dmac512_handle; // HAL handle for this tile
    volatile bool dmac512_busy;                // Transfer in progress flag
    uint32_t dmac512_transfer_id;             // Current transfer ID for interrupts
    
    // PLIC HAL integration - uses HAL's shared PLIC instances
    uint32_t plic_hart_id;             // Hart ID for this tile (maps to tile ID) 
    uint32_t plic_target_id;           // Target ID within PLIC for this hart
    uint8_t plic_instance;             // Which PLIC instance (0, 1, or 2)
    // NOTE: PLIC registers accessed via HAL's global PLIC_INST[] array, not per-tile pointers
    
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
    
    // PLIC interrupt statistics (optional monitoring)
    plic_interrupt_stats_t plic_stats;
    
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
// PLIC DEVICE AND HART INTERRUPT FUNCTIONS
// ============================================================================

// Platform PLIC support functions
int platform_init_plic_stats(mesh_platform_t* p);

// Device-side interrupt sources (hardware perspective)
// These represent peripheral devices asserting interrupt lines to PLIC
void device_task_completion_interrupt(uint32_t completing_hart_id, uint32_t task_id);
void device_dma_completion_interrupt(uint32_t source_hart_id, uint32_t transfer_id);
void device_error_interrupt(uint32_t source_hart_id, uint32_t error_code);
void device_timer_interrupt(uint32_t timer_id);
void device_resource_request_interrupt(uint32_t requesting_hart_id, uint32_t resource_id);
void device_shutdown_request_interrupt(uint32_t requesting_hart_id);

// Hart-side interrupt processing (CPU perspective)
// These handle interrupts delivered by PLIC to hart cores
int plic_process_hart_interrupts(uint32_t hart_id);

// Legacy functions for compatibility (use device functions instead)
int platform_trigger_task_complete(mesh_platform_t* p, uint32_t source_hart_id, uint32_t task_id);
int platform_trigger_dma_complete(mesh_platform_t* p, uint32_t source_hart_id, uint32_t transfer_id);
int platform_trigger_error(mesh_platform_t* p, uint32_t source_hart_id, uint32_t error_code);

#endif