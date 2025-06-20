#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include "parallel_noc_tests.h"
#include "hal_tests/hal_interface.h"
#include "mesh_noc/mesh_router.h"

// Thread-safe printing for parallel test execution
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

static void thread_safe_printf(const char* format, ...)
{
    pthread_mutex_lock(&print_mutex);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

static void thread_safe_banner(const char *msg)
{
    pthread_mutex_lock(&print_mutex);
    printf("================================\n");
    printf("# %s\n", msg);
    printf("================================\n");
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}





// Helper function for tile-to-C0 transfer task
int tile_to_c0_transfer_task(void* platform_ptr)
{
    // Auto-detect tile ID from thread local storage
    mesh_platform_t* p = (mesh_platform_t*)platform_ptr;
    if (!p) {
        printf("[ERROR] Platform pointer is NULL\n");
        return 0;
    }
    
    // Determine which tile this thread belongs to
    pthread_t current_thread = pthread_self();
    
    // Get the current executing tile ID from the platform context
    // This will be the tile that this task was assigned to
    int current_tile_id = -1;
    
    // Find which tile is executing this task by checking thread IDs
    for (int i = 1; i < p->node_count; i++) {
        if (pthread_equal(p->nodes[i].thread_id, current_thread)) {
            current_tile_id = i;
            break;
        }
    }
    
    if (current_tile_id == -1) {
        thread_safe_printf("[ERROR] Could not determine current tile ID\n");
        return 0;
    }
    
    thread_safe_printf("[Tile %d] Starting parallel transfer to C0...\n", current_tile_id);
    
    // Calculate source address for this tile
    uint64_t tile_bases[] = {
        TILE0_DLM1_512_BASE, TILE1_DLM1_512_BASE, TILE2_DLM1_512_BASE, TILE3_DLM1_512_BASE,
        TILE4_DLM1_512_BASE, TILE5_DLM1_512_BASE, TILE6_DLM1_512_BASE, TILE7_DLM1_512_BASE
    };
    
    uint64_t src_addr = tile_bases[current_tile_id];
    uint64_t dest_addr = DMEM0_512_BASE + 8192;  // Same destination for both transfers
    size_t size = 512;
    uint8_t pattern = 0x10 + current_tile_id;  // Unique pattern per tile
    
    // Prepare source data
    g_hal.memory_fill(src_addr, pattern, size);
    thread_safe_printf("[Tile %d] Source prepared with pattern 0x%02X\n", current_tile_id, pattern);
    
    // Execute transfer using HAL from this tile's context
    int result = g_hal.dma_remote_transfer(src_addr, dest_addr, size);
    
    thread_safe_printf("[Tile %d] Parallel transfer to C0 completed with result: %d\n", current_tile_id, result);
    return result;
}

int test_parallel_c0_access(mesh_platform_t* p)
{
    printf("================================\n");
    printf("# Parallel C0 Access Test - C0 Main Thread Orchestrator\n");
    printf("================================\n");
    
    // Enable NOC tracing
    extern int noc_trace_enabled;
    noc_trace_enabled = 1;
    
    printf("[C0-Orchestrator] Running on main C0 thread, coordinating tile threads directly\n");
    printf("[C0-Orchestrator] Selecting two tiles for parallel C0 access test\n");
    
         // Step 1: Clear destination (common for both transfers)
     uint64_t dest_addr = DMEM0_512_BASE + 8192;
     g_hal.memory_set(dest_addr, 0x00, 1024);
     
     printf("[C0-Orchestrator] Tasks will use tile_to_c0_transfer_task which auto-detects tile ID\n");
     printf("[C0-Orchestrator] Both transfers will target the same C0 destination simultaneously\n");
    
         // Step 2: Create tasks for parallel C0 transfers
     extern task_t* create_hal_test_task(mesh_platform_t* p, int (*test_func)(void*), const char* test_name, int* result_ptr);
     static int result1 = 0, result2 = 0;
     
     // Create tasks that will auto-detect their executing tile ID
     task_t* task1 = create_hal_test_task(p, tile_to_c0_transfer_task, "Parallel-C0-Transfer-A", &result1);
     task_t* task2 = create_hal_test_task(p, tile_to_c0_transfer_task, "Parallel-C0-Transfer-B", &result2);
    
    if (!task1 || !task2) {
        printf("[C0-Orchestrator] ERROR: Failed to create parallel transfer tasks!\n");
        return 0;
    }
    
              // Step 3: Queue tasks using existing system (round-robin will assign different tiles)
     extern int queue_task_to_available_tile(mesh_platform_t* p, task_t* task);
     
     // Queue the tasks - they will be assigned to consecutive tiles via round-robin
     int queue_result1 = queue_task_to_available_tile(p, task1);
     int queue_result2 = queue_task_to_available_tile(p, task2);
     
     if (queue_result1 != 0 || queue_result2 != 0) {
         printf("[C0-Orchestrator] ERROR: Failed to queue parallel tasks! (Results: %d, %d)\n", queue_result1, queue_result2);
         return 0;
     }
     
     printf("[C0-Orchestrator] Queued parallel tasks to available tile threads (round-robin assignment)\n");
         printf("[C0-Orchestrator] Waiting for tile threads to execute parallel transfers...\n");
     
     // Step 4: Wait for both tasks to complete by checking results directly
     int wait_cycles = 0;
     const int max_wait_cycles = 300; // 3 seconds max wait (300 * 10ms)
     
     while ((result1 == 0 || result2 == 0) && wait_cycles < max_wait_cycles) {
         usleep(10000); // 10ms check interval
         wait_cycles++;
         
         if (wait_cycles % 100 == 0) { // Print status every 1 second
             printf("[C0-Orchestrator] Still waiting... (Task A result: %d, Task B result: %d)\n", result1, result2);
         }
     }
     
     if (wait_cycles >= max_wait_cycles) {
         printf("[C0-Orchestrator] Timeout waiting for parallel transfers! (Task A: %d, Task B: %d)\n", result1, result2);
     }
     
     printf("[C0-Orchestrator] Parallel transfers completed!\n");
     printf("[C0-Orchestrator] Task A transfer result: %d\n", result1);
     printf("[C0-Orchestrator] Task B transfer result: %d\n", result2);
    
    // Verify results
    int success = (result1 > 0 && result2 > 0);
    printf("[C0-Orchestrator] Parallel C0 Access Test: %s\n", success ? "PASS" : "FAIL");
    
    return success;
}

 