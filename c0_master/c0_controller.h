#ifndef C0_CONTROLLER_H
#define C0_CONTROLLER_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

// STEP 1: Enhanced tile_core_t with basic threading infrastructure
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
    
} tile_core_t;

typedef struct {
    int id;
    uint64_t dmem_base_addr;    // Address space
    uint8_t* dmem_ptr;          // Simulated memory
    size_t dmem_size;
} dmem_module_t;

// STEP 1: Enhanced platform with threading (main thread = C0 master)
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
    
} mesh_platform_t;

// STEP 1: Thread management function declarations
int platform_start_tile_threads(mesh_platform_t* p);
int platform_stop_tile_threads(mesh_platform_t* p);
void* tile_processor_main(void* arg);

// C0 master functions (executed by main thread)
void c0_master_supervise_tiles(mesh_platform_t* p);
void c0_run_test_suite(mesh_platform_t* platform);

#endif