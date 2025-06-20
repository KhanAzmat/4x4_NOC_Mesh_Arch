#ifndef C0_CONTROLLER_H
#define C0_CONTROLLER_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

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
    
} mesh_platform_t;

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

#endif