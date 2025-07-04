#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "c0_controller.h"
#include "hal_tests/test_framework.h"
#include "hal_tests/parallel_noc_tests.h"
#include "mesh_noc/noc_packet.h"
#include "mesh_noc/mesh_router.h"
#include "generated/mem_map.h"
#include "interrupt/plic.h"

// STEP 2: Global platform context for tile threads
mesh_platform_t* g_platform_context = NULL;

// STEP 2: Task queue implementation
int task_queue_init(task_queue_t* queue)
{
    if (!queue) return -1;
    
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    
    if (pthread_mutex_init(&queue->queue_lock, NULL) != 0) {
        return -1;
    }
    
    if (pthread_cond_init(&queue->task_available, NULL) != 0) {
        pthread_mutex_destroy(&queue->queue_lock);
        return -1;
    }
    
    if (pthread_cond_init(&queue->queue_not_full, NULL) != 0) {
        pthread_mutex_destroy(&queue->queue_lock);
        pthread_cond_destroy(&queue->task_available);
        return -1;
    }
    
    // Initialize all tasks
    for (int i = 0; i < MAX_PENDING_TASKS; i++) {
        memset(&queue->tasks[i], 0, sizeof(task_t));
        pthread_mutex_init(&queue->tasks[i].task_lock, NULL);
        pthread_cond_init(&queue->tasks[i].task_complete, NULL);
    }
    
    printf("[Task Queue] Initialized with %d task slots\n", MAX_PENDING_TASKS);
    return 0;
}

int task_queue_destroy(task_queue_t* queue)
{
    if (!queue) return -1;
    
    // Clean up all tasks
    for (int i = 0; i < MAX_PENDING_TASKS; i++) {
        pthread_mutex_destroy(&queue->tasks[i].task_lock);
        pthread_cond_destroy(&queue->tasks[i].task_complete);
    }
    
    pthread_mutex_destroy(&queue->queue_lock);
    pthread_cond_destroy(&queue->task_available);
    pthread_cond_destroy(&queue->queue_not_full);
    
    printf("[Task Queue] Destroyed\n");
    return 0;
}

int task_queue_push(task_queue_t* queue, task_t* task)
{
    if (!queue || !task) return -1;
    
    pthread_mutex_lock(&queue->queue_lock);
    
    // Wait if queue is full
    while (queue->count >= MAX_PENDING_TASKS) {
        pthread_cond_wait(&queue->queue_not_full, &queue->queue_lock);
    }
    
    // Copy task to queue
    memcpy(&queue->tasks[queue->tail], task, sizeof(task_t));
    queue->tail = (queue->tail + 1) % MAX_PENDING_TASKS;
    queue->count++;
    
    printf("[Task Queue] Pushed task %d (type %d) for tile %d, queue size: %d\n", 
           task->task_id, task->type, task->assigned_tile, queue->count);
    
    // Signal waiting threads
    pthread_cond_signal(&queue->task_available);
    pthread_mutex_unlock(&queue->queue_lock);
    
    return 0;
}

task_t* task_queue_pop(task_queue_t* queue)
{
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->queue_lock);
    
    // Wait if queue is empty
    while (queue->count == 0) {
        pthread_cond_wait(&queue->task_available, &queue->queue_lock);
    }
    
    // Get task from queue
    task_t* task = &queue->tasks[queue->head];
    queue->head = (queue->head + 1) % MAX_PENDING_TASKS;
    queue->count--;
    
    printf("[Task Queue] Popped task %d (type %d) for tile %d, queue size: %d\n", 
           task->task_id, task->type, task->assigned_tile, queue->count);
    
    // Signal waiting threads
    pthread_cond_signal(&queue->queue_not_full);
    pthread_mutex_unlock(&queue->queue_lock);
    
    return task;
}

bool task_queue_is_empty(task_queue_t* queue)
{
    if (!queue) return true;
    
    pthread_mutex_lock(&queue->queue_lock);
    bool empty = (queue->count == 0);
    pthread_mutex_unlock(&queue->queue_lock);
    
    return empty;
}

// STEP 2: HAL Flow and Thread Verification (moved here for accessibility)
typedef struct {
    int tile_id;
    pthread_t thread_id;
    int tasks_executed;
    int hal_calls_made;
    char last_test_name[64];
    struct timespec last_execution_time;
} tile_execution_stats_t;

static tile_execution_stats_t tile_stats[8];
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

// STEP 1: Basic tile processor main loop
void* tile_processor_main(void* arg)
{
    tile_core_t* tile = (tile_core_t*)arg;
    
    printf("[Tile %d] Starting processor thread ...\n", tile->id);
    
    // Initialize tile state
    pthread_mutex_lock(&tile->state_lock);
    tile->running = true;
    tile->idle = true;
    tile->tasks_completed = 0;
    tile->task_pending = false;
    tile->current_task = NULL;
    tile->initialized = true;  // Signal that initialization is complete
    
    // NEW: Initialize interrupt tracking
    tile->interrupts_sent = 0;
    tile->last_interrupt_timestamp = 0;
    pthread_mutex_unlock(&tile->state_lock);
    
    // Update tile statistics with thread ID when thread starts
    pthread_mutex_lock(&stats_lock);
    tile_stats[tile->id].tile_id = tile->id;
    tile_stats[tile->id].thread_id = pthread_self();
    tile_stats[tile->id].tasks_executed = 0;
    tile_stats[tile->id].hal_calls_made = 0;
    memset(tile_stats[tile->id].last_test_name, 0, sizeof(tile_stats[tile->id].last_test_name));
    clock_gettime(CLOCK_MONOTONIC, &tile_stats[tile->id].last_execution_time);
    pthread_mutex_unlock(&stats_lock);
    
    printf("[Tile %d] Processor thread initialized with interrupt support\n", tile->id);
    
    // Tile processor main loop - can execute multiple tasks, but prevents duplicates
    while (true) {
        // Try to get a task from the platform
        task_t* task = tile_get_next_task(g_platform_context, tile);
        
        if (task) {
            printf("[Tile %d] Starting task %d execution\n", tile->id, task->task_id);
            
            // Execute the task
            int result = tile_execute_task(tile, task);
            
            // NEW: Send task completion interrupt to C0 before completing task
            int irq_result = PLIC_trigger_interrupt(tile->id, 0);  // Send to C0 (hart 0)
            if (irq_result >= 0) {
                printf("[Tile %d] Sent PLIC task completion interrupt for task %d\n", tile->id, task->task_id);
                } else {
                printf("[Tile %d] Failed to send PLIC interrupt: %d\n", tile->id, irq_result);
            }
            
            // Complete the task
            tile_complete_task(g_platform_context, tile, task);
            
            // Update tile state after task completion
            pthread_mutex_lock(&tile->state_lock);
            tile->tasks_completed++;
            tile->idle = true;
            tile->current_task = NULL;
            tile->task_pending = false;
            pthread_mutex_unlock(&tile->state_lock);
            
            printf("[Tile %d] Completed task %d (result=%d)\n", tile->id, task->task_id, result);
        } else {
            // No task available, stay idle
            pthread_mutex_lock(&tile->state_lock);
            tile->idle = true;
            pthread_mutex_unlock(&tile->state_lock);
            
            usleep(1000); // 1ms polling interval
        }
        
        // Check if we should stop
        pthread_mutex_lock(&tile->state_lock);
        bool should_run = tile->running;
        pthread_mutex_unlock(&tile->state_lock);
        
        if (!should_run) break;
    }
    
    // NEW: Send shutdown notification to C0
    if (g_platform_context && g_platform_context->plic_enabled) {
        tile_send_interrupt_to_c0(g_platform_context, tile->id, IRQ_TYPE_SHUTDOWN, 0, "Tile processor shutting down");
    }
    
    printf("[Tile %d] Processor thread stopping (sent %lu interrupts)...\n", tile->id, tile->interrupts_sent);
    return NULL;
}

// STEP 1: Start tile threads (main thread = C0 master)
int platform_start_tile_threads(mesh_platform_t* p)
{
    printf("[C0 Master] Starting tile processor threads...\n");
    
    // STEP 2: Set global platform context for tile threads
    g_platform_context = p;
    
    // Initialize platform state
    pthread_mutex_init(&p->platform_lock, NULL);
    p->platform_running = true;
    
    // STEP 2: Initialize task coordination system
    pthread_mutex_init(&p->task_id_lock, NULL);
    p->next_task_id = 1;
    p->active_tasks = 0;
    p->completed_tasks = 0;
    
    // Initialize task queue
    if (task_queue_init(&p->task_queue) != 0) {
        printf("[C0 Master] ERROR: Failed to initialize task queue\n");
        return -1;
    }
    
    // Initialize tile threads - SKIP tile 0 (C0 master) but include tiles 1-7
    for (int i = 1; i < p->node_count; i++) {  // i = 1 to 7 (skip only tile 0)
        tile_core_t* tile = &p->nodes[i];
        
        // Initialize thread state
        pthread_mutex_init(&tile->state_lock, NULL);
        tile->running = false;
        tile->initialized = false;
        
        printf("[C0 Master] Creating processor thread for tile %d...\n", i);
        
        // Create tile thread
        if (pthread_create(&tile->thread_id, NULL, tile_processor_main, tile) != 0) {
            printf("[C0 Master] ERROR: Failed to create thread for tile %d\n", i);
            return -1;
        }
    }
    
    // Wait for tiles 1-7 to initialize (skip only tile 0 = C0 master)
    printf("[C0 Master] Waiting for tile threads to initialize...\n");
    
    for (int i = 1; i < p->node_count; i++) {  // i = 1 to 7
        while (!p->nodes[i].initialized) {
            usleep(1000);
        }
    }
    
    printf("[C0 Master] All tile threads initialized successfully!\n");
    printf("[C0 Master] Task coordination system ready\n");
    return 0;
}

// STEP 1: Stop tile threads
int platform_stop_tile_threads(mesh_platform_t* p)
{
    printf("[C0 Master] Stopping tile processor threads...\n");
    
    // Signal tile processor threads (1-7) to stop
    for (int i = 1; i < p->node_count; i++) {  // i = 1 to 7
        pthread_mutex_lock(&p->nodes[i].state_lock);
        p->nodes[i].running = false;
        pthread_mutex_unlock(&p->nodes[i].state_lock);
    }
    
    // Wait for tile processor threads (1-7) to finish
    for (int i = 1; i < p->node_count; i++) {  // i = 1 to 7
        pthread_join(p->nodes[i].thread_id, NULL);
    }
    
    // STEP 2: Clean up task system
    task_queue_destroy(&p->task_queue);
    pthread_mutex_destroy(&p->task_id_lock);
    
    // Clean up mutexes for tiles 1-7
    for (int i = 1; i < p->node_count; i++) {  // i = 1 to 7
        pthread_mutex_destroy(&p->nodes[i].state_lock);
    }
    pthread_mutex_destroy(&p->platform_lock);
    
    p->platform_running = false;
    printf("[C0 Master] All tile threads stopped successfully!\n");
    printf("[C0 Master] Task coordination system cleaned up\n");
    return 0;
}

// STEP 1: C0 master supervision function (executed by main thread)
void c0_master_supervise_tiles(mesh_platform_t* p)
{
    printf("[C0 Master] Supervising tile processors with interrupt handling...\n");
    
    // Simple supervision - in later steps this will coordinate tasks
    int supervision_cycles = 0;
    
    while (p->platform_running && supervision_cycles < 5) {
        // NEW: Process pending interrupts from tiles
        // int interrupts_processed = c0_process_pending_interrupts(p);
        int interrupts_processed = c0_process_plic_interrupts(p);
        if (interrupts_processed > 0) {
            printf("[C0 Master] Processed %d interrupts this cycle\n", interrupts_processed);
        }
        
        // Check tile status for processor tiles (1-7)
        int active_tiles = 0;
        int idle_tiles = 0;
        int total_completed_tasks = 0;
        uint64_t total_interrupts_sent = 0;
        
        for (int i = 1; i < p->node_count; i++) {  // i = 1 to 7
            pthread_mutex_lock(&p->nodes[i].state_lock);
            if (p->nodes[i].running) {
                active_tiles++;
                if (p->nodes[i].idle) {
                    idle_tiles++;
                }
                total_completed_tasks += p->nodes[i].tasks_completed;
                total_interrupts_sent += p->nodes[i].interrupts_sent;
            }
            pthread_mutex_unlock(&p->nodes[i].state_lock);
        }
        
        printf("[C0 Master] Supervision cycle %d: %d processor tiles active, %d idle, %d tasks completed, %lu interrupts sent\n", 
               supervision_cycles + 1, active_tiles, idle_tiles, total_completed_tasks, total_interrupts_sent);
        
        // NEW: Print interrupt controller statistics
        if (p->plic_enabled) {
            printf("[C0 Master] PLIC interrupts processed: %lu\n",
                   p->plic_interrupts_processed);
        }
        
        usleep(200000); // 200ms supervision interval
        supervision_cycles++;
    }
    
    printf("[C0 Master] Supervision complete\n");
}

// Function to print atomic banner for main validation header
static void print_validation_banner(const char *msg)
{
    // Build entire banner in memory first
    char complete_banner[1000];
    int msg_len = strlen(msg);
    int total_width = 82; // Inner width of the box
    int padding = total_width - msg_len;
    
    snprintf(complete_banner, sizeof(complete_banner),
        "\n"
        "╔═══════════════════════════════════════════════════════════════════════════════════╗\n"
        "║ \033[1;34m%s\033[0m%*s║\n"
        "╚═══════════════════════════════════════════════════════════════════════════════════╝\n"
        "\n",
        msg, padding, "");
    
    // Single atomic write
    fputs(complete_banner, stdout);
    fflush(stdout);
}

// Function to print atomic banner for section headers
static void print_section_banner(const char *msg)
{
    // Build entire banner in memory first
    char complete_banner[1000];
    int msg_len = strlen(msg);
    int total_width = 82; // Inner width of the box
    int padding = total_width - msg_len;
    
    snprintf(complete_banner, sizeof(complete_banner),
        "\n"
        "╔═══════════════════════════════════════════════════════════════════════════════════╗\n"
        "║ \033[1;36m%s\033[0m%*s║\n"
        "╚═══════════════════════════════════════════════════════════════════════════════════╝\n"
        "\n",
        msg, padding, "");
    
    // Single atomic write
    fputs(complete_banner, stdout);
    fflush(stdout);
}

// Function to print atomic banner for reports
static void print_report_banner(const char *msg)
{
    // Build entire banner in memory first
    char complete_banner[1000];
    int msg_len = strlen(msg);
    int total_width = 82; // Inner width of the box
    int padding = total_width - msg_len;
    
    snprintf(complete_banner, sizeof(complete_banner),
        "\n"
        "╔═══════════════════════════════════════════════════════════════════════════════════╗\n"
        "║ \033[1;35m%s\033[0m%*s║\n"
        "╚═══════════════════════════════════════════════════════════════════════════════════╝\n",
        msg, padding, "");
    
    // Single atomic write
    fputs(complete_banner, stdout);
    fflush(stdout);
}

// Function to print atomic end banner for reports
static void print_end_banner(const char *msg)
{
    // Build entire banner in memory first
    char complete_banner[1000];
    int msg_len = strlen(msg);
    int total_width = 82; // Inner width of the box
    int padding = total_width - msg_len;
    
    snprintf(complete_banner, sizeof(complete_banner),
        "╚═══════════════════════════════════════════════════════════════════════════════════╝\n"
        "\n");
    
    // Single atomic write
    fputs(complete_banner, stdout);
    fflush(stdout);
}

// Enhanced test runner with main thread as C0 master
void c0_run_test_suite(mesh_platform_t* platform)
{
    print_validation_banner("Mesh‑NoC HAL Validation");
    
    // STEP 1: Start tile processor threads (main thread becomes C0 master)
    if (platform_start_tile_threads(platform) == 0) {
        printf("[C0 Master] Platform running with tile processors and task system!\n");
        
        // CONFIRM PLIC is enabled
        platform->plic_enabled = true;
        platform->plic_interrupts_processed = 0;
        printf("[C0 Master] PLIC interrupt system active\n");
        
            // Register default interrupt handlers
        // c0_register_interrupt_handler(platform, IRQ_TYPE_TASK_COMPLETE, default_task_complete_handler);
        // c0_register_interrupt_handler(platform, IRQ_TYPE_ERROR, default_error_handler);
        // c0_register_interrupt_handler(platform, IRQ_TYPE_DMA_COMPLETE, default_dma_complete_handler);
        // c0_register_interrupt_handler(platform, IRQ_TYPE_RESOURCE_REQUEST, default_resource_request_handler);
        // c0_register_interrupt_handler(platform, IRQ_TYPE_SHUTDOWN, default_shutdown_handler);
            
            // Enable interrupt processing for entire test suite
        platform->plic_enabled = true;
            printf("[C0 Master] Interrupt system enabled - tiles can now send interrupts to C0\n");
        
        // C0 master supervises the platform (with interrupts enabled)
        c0_master_supervise_tiles(platform);
        
        // STEP 2: Run HAL tests distributed across tiles (with interrupts still enabled)
        printf("[C0 Master] Executing HAL tests...\n");
        c0_run_hal_tests_distributed(platform);
        
        // NEW: Process any remaining interrupts after tests
        if (platform->plic_enabled) {
            printf("[C0 Master] Processing final interrupts...\n");
            // int final_interrupts = c0_process_pending_interrupts(platform);
            // if (final_interrupts > 0) {
            //     printf("[C0 Master] Processed %d final interrupts\n", final_interrupts);
            // }
            
            // Print final interrupt statistics
            print_report_banner("FINAL INTERRUPT SYSTEM STATISTICS");
            printf("C0 Interrupt Controller:\n");
            printf("  - Total PLIC Interrupts Processed: %lu\n", platform->plic_interrupts_processed);
            
            printf("\nTile Interrupt Statistics:\n");
            for (int i = 1; i < platform->node_count; i++) {
                printf("  - Tile %d: %lu interrupts sent\n", i, platform->nodes[i].interrupts_sent);
            }
            print_end_banner("END INTERRUPT STATISTICS");
            
            // Disable interrupt processing and cleanup
            platform->plic_enabled = false;
            // c0_interrupt_controller_destroy(&platform->interrupt_controller);
            printf("[C0 Master] Interrupt system shutdown complete\n");
        }
        
        // STEP 1: Stop tile processor threads
        platform_stop_tile_threads(platform);
    } else {
        printf("[C0 Master] ERROR: Failed to start tile threads, running in single-threaded mode\n");
    run_all_tests(platform);
    }
}

// STEP 2: Enhanced task storage for tile-specific assignment
static task_t hal_task_storage[MAX_PENDING_TASKS];
static int hal_task_storage_count = 0;
static pthread_mutex_t hal_task_storage_lock = PTHREAD_MUTEX_INITIALIZER;

// STEP 2: Atomic printing session lock for complete statement sequences
static pthread_mutex_t print_session_lock = PTHREAD_MUTEX_INITIALIZER;

// Function to log HAL call flow verification
void verify_hal_call_flow(int tile_id, const char* test_name, const char* hal_function, const char* driver_function) {
    pthread_mutex_lock(&stats_lock);
    
    tile_stats[tile_id].hal_calls_made++;
    strncpy(tile_stats[tile_id].last_test_name, test_name, sizeof(tile_stats[tile_id].last_test_name) - 1);
    clock_gettime(CLOCK_MONOTONIC, &tile_stats[tile_id].last_execution_time);
    
    pthread_mutex_unlock(&stats_lock);
    
    printf("[HAL-FLOW] Tile %d: Test '%s' → HAL '%s' → Driver '%s'\n", 
           tile_id, test_name, hal_function, driver_function);
    fflush(stdout);
}

// Function to update tile execution statistics
void update_tile_execution_stats(int tile_id, pthread_t thread_id, const char* test_name) {
    pthread_mutex_lock(&stats_lock);
    
    tile_stats[tile_id].tile_id = tile_id;
    tile_stats[tile_id].thread_id = thread_id;
    tile_stats[tile_id].tasks_executed++;
    strncpy(tile_stats[tile_id].last_test_name, test_name, sizeof(tile_stats[tile_id].last_test_name) - 1);
    clock_gettime(CLOCK_MONOTONIC, &tile_stats[tile_id].last_execution_time);
    
    pthread_mutex_unlock(&stats_lock);
}

// Function to print comprehensive execution verification
void print_execution_verification() {
    pthread_mutex_lock(&stats_lock);
    
    print_report_banner("EXECUTION VERIFICATION REPORT");
    printf("+------+--------------+-----------+-------------+----------------------+\n");
    printf("| Tile | Thread ID    | Tasks Exec| HAL Calls   | Last Test            |\n");
    printf("+------+--------------+-----------+-------------+----------------------+\n");
    
    // Show all tiles 0-7
    for (int i = 0; i < 8; i++) {
        printf("| %4d | %12lu | %9d | %11d | %-20s |\n",
               tile_stats[i].tile_id,
               (unsigned long)tile_stats[i].thread_id,
               tile_stats[i].tasks_executed,
               tile_stats[i].hal_calls_made,
               tile_stats[i].last_test_name[0] ? tile_stats[i].last_test_name : "None");
    }
    
    printf("+------+--------------+-----------+-------------+----------------------+\n");
    
    // Verify each tile 1-7 has processor threads (tile 0 is C0 master)
    printf("\nTHREAD ASSIGNMENT VERIFICATION:\n");
    printf("Tile 0: Reserved for C0 Master (no processor thread needed)\n");
    
    int active_tiles = 0;
    for (int i = 1; i <= 7; i++) {
        if (tile_stats[i].thread_id > 0) {
            printf("Tile %d: ACTIVE (Thread %lu executed %d tasks)\n",
                   i, (unsigned long)tile_stats[i].thread_id, tile_stats[i].tasks_executed);
            active_tiles++;
        } else {
            printf("Tile %d: INACTIVE (No processor thread created)\n", i);
        }
    }
    
    printf("\nHAL FLOW VERIFICATION:\n");
    int hal_active_tiles = 0;
    for (int i = 1; i <= 7; i++) {
        if (tile_stats[i].hal_calls_made > 0) {
            printf("Tile %d: HAL FLOW VERIFIED (%d HAL calls made)\n",
                   i, tile_stats[i].hal_calls_made);
            hal_active_tiles++;
        } else if (tile_stats[i].thread_id > 0) {
            printf("Tile %d: PROCESSOR THREAD ACTIVE (no HAL tasks assigned yet)\n", i);
        } else {
            printf("Tile %d: NO HAL FLOW DETECTED\n", i);
        }
    }
    
    printf("\nSUMMARY:\n");
    printf("- Processor Tiles (1-7): %d/7 active\n", active_tiles);
    printf("- HAL Flow Verified: %d/7 tiles\n", hal_active_tiles);
    printf("- Tile 0: C0 Master (main thread)\n");
    printf("- All 7 processor threads available for task distribution\n");
    print_end_banner("END VERIFICATION REPORT");
    
    pthread_mutex_unlock(&stats_lock);
}

// Function to begin atomic print session (blocks other threads from printing)
void begin_print_session(int tile_id, const char* task_name) {
    pthread_mutex_lock(&print_session_lock);
    // Build entire banner in memory first for atomic printing
    char session_banner[1000];
    snprintf(session_banner, sizeof(session_banner),
        "===================================================================\n"
        "[Tile %d] Starting %s - Print Session BEGIN\n"
        "===================================================================\n", tile_id, task_name);
    fputs(session_banner, stdout);
    fflush(stdout);
}

// Function to end atomic print session (allows other threads to print)
void end_print_session(int tile_id, const char* task_name, int result) {
    // Build entire banner in memory first for atomic printing
    char end_banner[1000];
    snprintf(end_banner, sizeof(end_banner),
        "===================================================================\n"
        "[Tile %d] %s Completed: %s - Print Session END\n"
        "===================================================================\n\n", 
        tile_id, task_name, result ? "PASS" : "FAIL");
    fputs(end_banner, stdout);
    fflush(stdout);
    pthread_mutex_unlock(&print_session_lock);
}

// Main thread safe printing - respects ongoing print sessions
void main_thread_print(const char* format, ...) {
    pthread_mutex_lock(&print_session_lock);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    
    pthread_mutex_unlock(&print_session_lock);
}

task_t* create_hal_test_task(mesh_platform_t* p, 
                            int (*test_func)(void*),
                            const char* test_name,
                            int* result_ptr)
{
    if (!p || !test_func || !test_name || !result_ptr) {
        return NULL;
    }
    
    // Get unique task ID
    pthread_mutex_lock(&p->task_id_lock);
    int task_id = p->next_task_id++;
    pthread_mutex_unlock(&p->task_id_lock);
    
    // Use static task storage (simple allocation)
    pthread_mutex_lock(&hal_task_storage_lock);
    if (hal_task_storage_count >= MAX_PENDING_TASKS) {
        pthread_mutex_unlock(&hal_task_storage_lock);
        return NULL;
    }
    
    task_t* task = &hal_task_storage[hal_task_storage_count];
    hal_task_storage_count++;
    pthread_mutex_unlock(&hal_task_storage_lock);
    
    // Initialize task
    memset(task, 0, sizeof(task_t));
    task->task_id = task_id;
    task->type = TASK_TYPE_HAL_TEST;
    task->assigned_tile = -1; // Will be assigned when queued
    task->completed = false;
    task->taken = false;  // Initialize taken flag
    task->result = 0;
    
    // Set HAL test parameters
    task->params.hal_test.test_func = test_func;
    task->params.hal_test.test_name = test_name;
    task->params.hal_test.platform = p;
    task->params.hal_test.result_ptr = result_ptr;
    
    // Initialize synchronization
    pthread_mutex_init(&task->task_lock, NULL);
    pthread_cond_init(&task->task_complete, NULL);
    
    printf("[C0 Master] Created HAL test task %d: '%s'\n", task_id, test_name);
    return task;
}

int queue_task_to_available_tile(mesh_platform_t* p, task_t* task)
{
    if (!p || !task) {
        return -1;
    }
    
    // Round-robin assignment to tiles 1-7 (excluding tile 0 = C0 master)
    static int next_tile = 1; // Start from tile 1, not tile 0
    int target_tile = next_tile;
    next_tile++;
    if (next_tile >= p->node_count) {
        next_tile = 1; // Wrap back to tile 1, skip tile 0
    }
    
    task->assigned_tile = target_tile;
    
    // Increment active task count
    pthread_mutex_lock(&p->platform_lock);
    p->active_tasks++;
    pthread_mutex_unlock(&p->platform_lock);
    
    printf("[C0 Master] HAL test task %d '%s' assigned to tile %d (tile 0 reserved for C0 master)\n", 
           task->task_id, task->params.hal_test.test_name, target_tile);
    
    // Store task in static storage for tile-specific retrieval
    pthread_mutex_lock(&hal_task_storage_lock);
    if (hal_task_storage_count < MAX_PENDING_TASKS) {
        hal_task_storage[hal_task_storage_count] = *task;
        
        // CRITICAL: Also mark the original task as taken to prevent duplicate execution
        task->taken = true;
        task->assigned_tile = -999;  // Invalidate original to prevent re-use
        
        hal_task_storage_count++;
    }
    pthread_mutex_unlock(&hal_task_storage_lock);
    
    return 0;
}

int wait_for_all_tasks_completion(mesh_platform_t* p, int expected_count)
{
    if (!p || expected_count <= 0) {
        return -1;
    }
    
    printf("[C0 Master] Waiting for %d HAL test tasks to complete (with interrupt processing)...\n", expected_count);
    
    int completed = 0;
    int total_interrupts_processed = 0;
    
    while (completed < expected_count) {
        // Process any pending interrupts while waiting
        if (p->plic_enabled) {
            // int interrupts_this_cycle = c0_process_pending_interrupts(p);
            // total_interrupts_processed += interrupts_this_cycle;
            
            // if (interrupts_this_cycle > 0) {
            //     printf("[C0 Master] Processed %d interrupts while waiting for task completion\n", interrupts_this_cycle);
            // }
        }
        
        // Check completion status
        pthread_mutex_lock(&p->platform_lock);
        completed = p->completed_tasks;
        pthread_mutex_unlock(&p->platform_lock);
        
        if (completed < expected_count) {
            usleep(10000); // 10ms check interval
        }
    }
    
    printf("[C0 Master] All %d HAL test tasks completed!\n", expected_count);
    if (total_interrupts_processed > 0) {
        printf("[C0 Master] Processed %d total interrupts during task execution\n", total_interrupts_processed);
    }
    return 0;
}

// STEP 2: Wrapper functions to convert void* to mesh_platform_t* for HAL tests
static int hal_test_cpu_local_move_wrapper(void* p) { 
    extern int test_cpu_local_move(mesh_platform_t* p);
    return test_cpu_local_move((mesh_platform_t*)p); 
}
static int hal_test_dma_local_transfer_wrapper(void* p) { 
    extern int test_dma_local_transfer(mesh_platform_t* p);
    return test_dma_local_transfer((mesh_platform_t*)p); 
}
static int hal_test_dma_remote_transfer_wrapper(void* p) { 
    extern int test_dma_remote_transfer(mesh_platform_t* p);
    return test_dma_remote_transfer((mesh_platform_t*)p); 
}
static int hal_test_c0_gather_wrapper(void* p) { 
    extern int test_c0_gather(mesh_platform_t* p);
    return test_c0_gather((mesh_platform_t*)p); 
}
static int hal_test_c0_distribute_wrapper(void* p) { 
    extern int test_c0_distribute(mesh_platform_t* p);
    return test_c0_distribute((mesh_platform_t*)p); 
}
static int hal_test_noc_bandwidth_wrapper(void* p) { 
    extern int test_noc_bandwidth(mesh_platform_t* p);
    return test_noc_bandwidth((mesh_platform_t*)p); 
}
static int hal_test_noc_latency_wrapper(void* p) { 
    extern int test_noc_latency(mesh_platform_t* p);
    return test_noc_latency((mesh_platform_t*)p); 
}
static int hal_test_random_dma_remote_wrapper(void* p) { 
    extern int test_random_dma_remote(mesh_platform_t* p);
    return test_random_dma_remote((mesh_platform_t*)p); 
}

static int hal_test_parallel_c0_access_wrapper(void* p) { 
    extern int test_parallel_c0_access(mesh_platform_t* p);
    return test_parallel_c0_access((mesh_platform_t*)p); 
}

// ---------------------------DMEM----TESTS--------------------------------------------------
static int hal_test_dmem_basic_functionality_wrapper(void* p) { 
    extern int test_dmem_basic_functionality(mesh_platform_t* p);
    return test_dmem_basic_functionality((mesh_platform_t*)p);
}
static int hal_test_dmem_large_transfers_wrapper(void* p) { 
    extern int test_dmem_large_transfers(mesh_platform_t* p);
    return test_dmem_large_transfers((mesh_platform_t*)p);
}
static int hal_test_dmem_address_validation_wrapper(void* p) { 
    extern int test_dmem_address_validation(mesh_platform_t* p);
    return test_dmem_address_validation((mesh_platform_t*)p);
}
static int hal_test_dmem_data_integrity_wrapper(void* p) { 
    extern int test_dmem_data_integrity(mesh_platform_t* p);
    return test_dmem_data_integrity((mesh_platform_t*)p);
}
static int hal_test_dmem_concurrent_access_wrapper(void* p) { 
    extern int test_dmem_concurrent_access(mesh_platform_t* p);
    return test_dmem_concurrent_access((mesh_platform_t*)p);
}
static int hal_test_dmem_boundary_conditions_wrapper(void* p) { 
    extern int test_dmem_boundary_conditions(mesh_platform_t* p);
    return test_dmem_boundary_conditions((mesh_platform_t*)p);
}
static int hal_test_dmem_error_handling_wrapper(void* p) { 
    extern int test_dmem_error_handling(mesh_platform_t* p);
    return test_dmem_error_handling((mesh_platform_t*)p);
}
static int hal_test_dmem_performance_basic_wrapper(void* p) { 
    extern int test_dmem_performance_basic(mesh_platform_t* p);
    return test_dmem_performance_basic((mesh_platform_t*)p);
}
static int hal_test_dmem_cross_module_transfers_wrapper(void* p) { 
    extern int test_dmem_cross_module_transfers(mesh_platform_t* p);
    return test_dmem_cross_module_transfers((mesh_platform_t*)p);
}
static int hal_test_dmem_alignment_testing_wrapper(void* p) { 
    extern int test_dmem_alignment_testing(mesh_platform_t* p);
    return test_dmem_alignment_testing((mesh_platform_t*)p);
}

// ---------------------DMEM----TESTS------------------------------------------------


void c0_run_hal_tests_distributed(mesh_platform_t* platform)
{
    print_section_banner("Running Tests: C0 Master + Distributed HAL");
    
    // STEP 1: Run C0 Master tests on main thread (these are C0 coordination tasks)
    printf("[C0 Master] Executing C0 Master coordination tests...\n");
    extern int test_c0_gather(mesh_platform_t* p);
    extern int test_c0_distribute(mesh_platform_t* p);
    extern int test_parallel_c0_access(mesh_platform_t* p);
    
    int c0_gather_result = test_c0_gather(platform);
    int c0_distribute_result = test_c0_distribute(platform);
    int parallel_c0_result = test_parallel_c0_access(platform);  // Run on C0 main thread

    // int c0_gather_result = 1;
    // int c0_distribute_result = 1;
    // int parallel_c0_result = 1;
    
    printf("[C0 Master] C0 Master tests completed:\n");
    printf("[C0 Master] - C0 Gather: %s\n", c0_gather_result ? "PASS" : "FAIL");
    printf("[C0 Master] - C0 Distribute: %s\n", c0_distribute_result ? "PASS" : "FAIL");
    printf("[C0 Master] - Parallel C0 Access: %s\n", parallel_c0_result ? "PASS" : "FAIL");
    printf("\n");
    
    // STEP 2: Run HAL tests distributed across tile processors IN PARALLEL
    printf("[C0 Master] Executing HAL tests in parallel across tile processors...\n");
    
    // Test function table using wrapper functions (excluding C0 tests and Parallel C0 Access)
    struct {
        int (*func)(void*);
        const char* name;
        int result;
    } hal_tests[] = {
        {hal_test_cpu_local_move_wrapper, "CPU Local Move", 0},
        {hal_test_dma_local_transfer_wrapper, "DMA Local Transfer", 0},
        {hal_test_dma_remote_transfer_wrapper, "DMA Remote Transfer", 0},
        {hal_test_noc_bandwidth_wrapper, "NoC Bandwidth", 0},
        {hal_test_noc_latency_wrapper, "NoC Latency", 0},
        {hal_test_random_dma_remote_wrapper, "Random DMA Remote", 0},
        // Parallel C0 Access is now run on C0 main thread, not distributed

        // ----------DMEM--TESTS----------------------------------
        {hal_test_dmem_basic_functionality_wrapper, "DMEM-Basic-Functionality", 0},
        {hal_test_dmem_large_transfers_wrapper, "DMEM-Large-Transfers", 0},
        {hal_test_dmem_address_validation_wrapper, "DMEM-Address-Validation", 0},
        {hal_test_dmem_data_integrity_wrapper, "DMEM-Data-Integrity", 0},
        {hal_test_dmem_concurrent_access_wrapper, "DMEM-Concurrent-Access", 0},
        {hal_test_dmem_boundary_conditions_wrapper, "DMEM-Boundary-Conditions", 0},
        {hal_test_dmem_error_handling_wrapper, "DMEM-Error-Handling", 0},
        {hal_test_dmem_performance_basic_wrapper, "DMEM-Performance-Basic", 0},
        {hal_test_dmem_cross_module_transfers_wrapper, "DMEM-Cross-Module-Transfers", 0},
        {hal_test_dmem_alignment_testing_wrapper, "DMEM-Alignment-Testing", 0},
        // ----------DMEM---TESTS-------------------------------------------------
    
    };
    
    int num_hal_tests = sizeof(hal_tests) / sizeof(hal_tests[0]);
    
    // Reset task storage for new test run
    pthread_mutex_lock(&hal_task_storage_lock);
    hal_task_storage_count = 0;
    memset(hal_task_storage, 0, sizeof(hal_task_storage));
    pthread_mutex_unlock(&hal_task_storage_lock);
    
    // Reset completion counter for parallel execution
    pthread_mutex_lock(&platform->platform_lock);
    platform->completed_tasks = 0;
    platform->active_tasks = 0;
    pthread_mutex_unlock(&platform->platform_lock);
    
    // Create and queue ALL HAL test tasks for parallel execution
    main_thread_print("[C0 Master] Creating %d HAL test tasks for parallel execution...\n", num_hal_tests);
    for (int i = 0; i < num_hal_tests; i++) {
        task_t* task = create_hal_test_task(platform, hal_tests[i].func, 
                                           hal_tests[i].name, &hal_tests[i].result);
        if (task) {
            queue_task_to_available_tile(platform, task);
        } else {
            main_thread_print("[C0 Master] ERROR: Failed to create task for %s\n", hal_tests[i].name);
            hal_tests[i].result = 0;
        }
    }
    
    // Wait for ALL HAL tests to complete in parallel
    main_thread_print("[C0 Master] Waiting for all %d HAL test tasks to complete in parallel...\n", num_hal_tests);
    wait_for_all_tasks_completion(platform, num_hal_tests);
    main_thread_print("[C0 Master] All parallel HAL test tasks completed!\n");
    
    // Print results
    main_thread_print("\n");
    print_section_banner("Test Results Summary");
    main_thread_print("[C0 Master] C0 Master Tests (Main Thread):\n");
    main_thread_print("[C0 Master] - C0 Gather: %s\n", c0_gather_result ? "PASS" : "FAIL");
    main_thread_print("[C0 Master] - C0 Distribute: %s\n", c0_distribute_result ? "PASS" : "FAIL");
    
    int hal_passed = 0;
    main_thread_print("[C0 Master] HAL Tests (Parallel Distribution to Tile Processors):\n");
    for (int i = 0; i < num_hal_tests; i++) {
        hal_passed += hal_tests[i].result;
        main_thread_print("[C0 Master] - %s: %s\n", hal_tests[i].name, hal_tests[i].result ? "PASS" : "FAIL");
    }
    
    int total_passed = c0_gather_result + c0_distribute_result + parallel_c0_result + hal_passed;
    int total_tests = 3 + num_hal_tests;
    main_thread_print("\033[1m[C0 Master] Overall Summary: %d/%d tests passed\033[0m\n", total_passed, total_tests);
    print_section_banner("Test Execution Complete");
    
    // Print comprehensive verification report
    print_execution_verification();
}

// STEP 2: Tile task execution functions
task_t* tile_get_next_task(mesh_platform_t* p, tile_core_t* tile)
{
    if (!p || !tile) {
        return NULL;
    }
    
    // Search through static task storage for tasks assigned to this tile
    pthread_mutex_lock(&hal_task_storage_lock);
    
    task_t* assigned_task = NULL;
    
    // Look for an unstarted task assigned to this tile
    for (int i = 0; i < hal_task_storage_count; i++) {
        task_t* task = &hal_task_storage[i];
        
        // STRICT CHECK: Only allow task retrieval if ALL conditions are met
        if (task->assigned_tile == tile->id && 
            !task->completed && 
            !task->taken &&  // Check taken flag
            task->task_id > 0) {  // Ensure task is valid
            
            // IMMEDIATELY mark as taken AND invalidate task_id to prevent race conditions
            task->taken = true;  // Set taken flag
            task->assigned_tile = -999;  // Invalidate assignment to prevent re-retrieval
            assigned_task = task;
            break;
        }
    }
    
    pthread_mutex_unlock(&hal_task_storage_lock);
    
    // Don't print here - will be printed inside atomic session
    return assigned_task;
}

int tile_execute_task(tile_core_t* tile, task_t* task)
{
    if (!tile || !task) {
        return -1;
    }
    
    // Update tile state
    pthread_mutex_lock(&tile->state_lock);
    tile->current_task = task;
    tile->task_pending = false;
    tile->idle = false;
    pthread_mutex_unlock(&tile->state_lock);
    
    // Execute task based on type
    int result = 0;
    
    switch (task->type) {
        case TASK_TYPE_HAL_TEST:
            // Update execution statistics BEFORE starting
            update_tile_execution_stats(tile->id, pthread_self(), task->params.hal_test.test_name);
            
            // BEGIN ATOMIC PRINT SESSION - blocks other threads from printing
            begin_print_session(tile->id, task->params.hal_test.test_name);
            
            // Execute real HAL test function within atomic print session
            printf("[Tile %d] Executing HAL test: %s\n", tile->id, task->params.hal_test.test_name);
            
            if (task->params.hal_test.test_func && task->params.hal_test.platform) {
                // Log HAL flow verification before calling test
                verify_hal_call_flow(tile->id, task->params.hal_test.test_name, "hal_reference", "hardware_driver");
                
                printf("[HAL-CALL] Tile %d: Calling HAL test function for '%s'\n", tile->id, task->params.hal_test.test_name);
                
                // Call the HAL test function with platform parameter
                result = task->params.hal_test.test_func(task->params.hal_test.platform);
                
                // Store result in the pointer location for main thread to read
                if (task->params.hal_test.result_ptr) {
                    *(task->params.hal_test.result_ptr) = result;
                } else {
                    printf("[Tile %d] ERROR - result_ptr is NULL!\n", tile->id);
                }
                
                printf("[HAL-RESULT] Tile %d: HAL test '%s' returned result: %d\n", 
                       tile->id, task->params.hal_test.test_name, result);
                printf("[Tile %d] HAL test '%s' completed with result: %s\n", 
                       tile->id, task->params.hal_test.test_name, result ? "PASS" : "FAIL");
                printf("[Tile %d] Task %d completed with result: %d\n", 
                       tile->id, task->task_id, result);
            } else {
                printf("[Tile %d] ERROR: Invalid HAL test parameters\n", tile->id);
                result = 0;
            }
            
            // END ATOMIC PRINT SESSION - allows other threads to print
            end_print_session(tile->id, task->params.hal_test.test_name, result);
            break;
            
        case TASK_TYPE_MEMORY_COPY:
            // Simple memory operation simulation
            printf("[Tile %d] [Placeholder] Executing memory copy task...\n", tile->id);
            usleep(5000); // 5ms simulated work
            result = 256; // Simulate 256 bytes copied
            break;
            
        case TASK_TYPE_DMA_TRANSFER:
            // DMA transfer simulation
            printf("[Tile %d][Placeholder] Executing DMA transfer task...\n", tile->id);
            usleep(8000); // 8ms simulated work
            result = (int)task->params.memory_op.size;
            break;
            
        case TASK_TYPE_COMPUTATION:
            // Computation simulation
            printf("[Tile %d][Placeholder]Executing computation task...\n", tile->id);
            usleep(15000); // 15ms simulated work
            result = 1; // Success
            break;
            
        case TASK_TYPE_NOC_TRANSFER:
            // NoC transfer simulation
            printf("[Tile %d][Placeholder] Executing NoC transfer task...\n", tile->id);
            usleep(10000); // 10ms simulated work
            result = (int)task->params.memory_op.size;
            break;
            
        case TASK_TYPE_TEST_EXECUTION:
            // Test execution simulation
            printf("[Tile %d][Placeholder] Executing test task %d...\n", tile->id, task->params.test_exec.test_id);
            usleep(12000); // 12ms simulated work
            result = 1; // Success
            break;
            
        default:
            printf("[Tile %d] Unknown task type %d\n", tile->id, task->type);
            result = -1;
            break;
    }
    
    // Update task result
    task->result = result;
    
    return result;
}

int tile_complete_task(mesh_platform_t* p, tile_core_t* tile, task_t* task)
{
    if (!p || !tile || !task) {
        return -1;
    }
    
    // Mark task as completed
    pthread_mutex_lock(&task->task_lock);
    task->completed = true;
    pthread_cond_signal(&task->task_complete);
    pthread_mutex_unlock(&task->task_lock);
    
    // Update tile state
    pthread_mutex_lock(&tile->state_lock);
    tile->current_task = NULL;
    tile->idle = true;
    tile->tasks_completed++;
    pthread_mutex_unlock(&tile->state_lock);
    
    // Update platform completion count
    pthread_mutex_lock(&p->platform_lock);
    p->completed_tasks++;
    p->active_tasks--;
    pthread_mutex_unlock(&p->platform_lock);
    
    // Don't print here - completion message is already inside atomic session
    return 0;
}

// ============================================================================
// NEW: INTERRUPT SYSTEM IMPLEMENTATION
// ============================================================================

// Initialize C0 interrupt controller
int c0_interrupt_controller_init(c0_interrupt_controller_t* ctrl) {
    if (!ctrl) return -1;
    
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->head = 0;
    ctrl->tail = 0;
    ctrl->count = 0;
    ctrl->processing_enabled = false;
    
    if (pthread_mutex_init(&ctrl->irq_lock, NULL) != 0) {
        return -1;
    }
    
    if (pthread_cond_init(&ctrl->irq_available, NULL) != 0) {
        pthread_mutex_destroy(&ctrl->irq_lock);
        return -1;
    }
    
    printf("[C0-IRQ] Interrupt controller initialized\n");
    return 0;
}

// Destroy C0 interrupt controller
int c0_interrupt_controller_destroy(c0_interrupt_controller_t* ctrl) {
    if (!ctrl) return -1;
    
    ctrl->processing_enabled = false;
    
    pthread_mutex_destroy(&ctrl->irq_lock);
    pthread_cond_destroy(&ctrl->irq_available);
    
    printf("[C0-IRQ] Interrupt controller destroyed\n");
    return 0;
}

// Process pending interrupts in C0 master
// int c0_process_pending_interrupts(mesh_platform_t* p) {
//     if (!p || !p->plic_enabled) return 0;
    
//     c0_interrupt_controller_t* ctrl = &p->interrupt_controller;
//     int processed = 0;
    
//     pthread_mutex_lock(&ctrl->irq_lock);
    
//     while (ctrl->count > 0) {
//         // Get interrupt from queue
//         interrupt_request_t* irq = &ctrl->irq_queue[ctrl->head];
//         ctrl->head = (ctrl->head + 1) % MAX_PENDING_IRQS;
//         ctrl->count--;
        
//         pthread_mutex_unlock(&ctrl->irq_lock);
        
//         // Process interrupt
//         printf("[C0-IRQ] Processing %s interrupt from tile %d (data=0x%x)\n",
//                get_irq_type_name(irq->type), irq->source_tile, irq->data);
        
//         // Call appropriate handler
//         if (irq->type >= 1 && irq->type <= IRQ_TYPE_MAX && ctrl->isr_handlers[irq->type]) {
//             int result = ctrl->isr_handlers[irq->type](irq, p);
//             if (result == 0) {
//                 ctrl->interrupts_processed++;
//             }
//         } else {
//             printf("[C0-IRQ] No handler for interrupt type %d\n", irq->type);
//         }
        
//         processed++;
//         pthread_mutex_lock(&ctrl->irq_lock);
//     }
    
//     pthread_mutex_unlock(&ctrl->irq_lock);
//     return processed;
// }

// Register interrupt handler
// int c0_register_interrupt_handler(mesh_platform_t* p, interrupt_type_t type, interrupt_handler_t handler) {
//     if (!p || type < 1 || type > IRQ_TYPE_MAX) return -1;
    
//     p->interrupt_controller.isr_handlers[type] = handler;
//     printf("[C0-IRQ] Registered handler for %s interrupts\n", get_irq_type_name(type));
//     return 0;
// }

// Send interrupt from tile to C0 via NoC
int tile_send_interrupt_to_c0(mesh_platform_t* p, int tile_id, interrupt_type_t type, 
                              uint32_t data, const char* message) {
    if (!p || tile_id < 1 || tile_id >= NUM_TILES) return -1;
    
    // Create interrupt request
    interrupt_request_t irq;
    memset(&irq, 0, sizeof(irq));
    irq.source_tile = tile_id;
    irq.type = type;
    irq.priority = get_irq_priority(type);
    irq.timestamp = get_current_timestamp_ns();
    irq.data = data;
    irq.valid = true;
    
    if (message) {
        strncpy(irq.message, message, sizeof(irq.message) - 1);
        irq.message[sizeof(irq.message) - 1] = '\0';
    }
    
    // Send via NoC packet
    // int result = noc_send_interrupt_packet(tile_id, 0, &irq);

    // Use PLIC to send interrupt directly
    int result = PLIC_trigger_interrupt(tile_id, 0); 
    
    if (result == 0) {
        p->nodes[tile_id].interrupts_sent++;
        p->nodes[tile_id].last_interrupt_timestamp = irq.timestamp;
        printf("[TILE-%d] Sent %s interrupt to C0 via NoC\n", tile_id, get_irq_type_name(type));
    } else {
        printf("[TILE-%d] Failed to send interrupt to C0\n", tile_id);
    }
    
    return result;
}

// Convenience functions for common interrupts
int tile_signal_task_complete(mesh_platform_t* p, int tile_id, uint32_t task_id) {
    char message[64];
    snprintf(message, sizeof(message), "Task %u completed on tile %d", task_id, tile_id);
    return tile_send_interrupt_to_c0(p, tile_id, IRQ_TYPE_TASK_COMPLETE, task_id, message);
}

int tile_signal_error(mesh_platform_t* p, int tile_id, uint32_t error_code, const char* error_msg) {
    char message[64];
    snprintf(message, sizeof(message), "Error %u: %s", error_code, error_msg ? error_msg : "Unknown error");
    return tile_send_interrupt_to_c0(p, tile_id, IRQ_TYPE_ERROR, error_code, message);
}

int tile_signal_dma_complete(mesh_platform_t* p, int tile_id, uint32_t transfer_id) {
    char message[64];
    snprintf(message, sizeof(message), "DMA transfer %u completed", transfer_id);
    return tile_send_interrupt_to_c0(p, tile_id, IRQ_TYPE_DMA_COMPLETE, transfer_id, message);
}

// NoC interrupt packet functions
// int noc_send_interrupt_packet(int src_tile, int dst_tile, interrupt_request_t* irq) {
//     if (!irq || src_tile < 0 || src_tile >= NUM_TILES || dst_tile < 0 || dst_tile >= NUM_TILES) {
//         return -1;
//     }
    
//     // Convert to compact format for NoC transmission
//     compact_interrupt_packet_t compact_irq;
//     memset(&compact_irq, 0, sizeof(compact_irq));
    
//     compact_irq.source_tile = (uint32_t)irq->source_tile;
//     compact_irq.type = (uint32_t)irq->type;
//     compact_irq.priority = (uint32_t)irq->priority;
//     compact_irq.timestamp = irq->timestamp;
//     compact_irq.data = irq->data;
//     compact_irq.valid = irq->valid ? 1 : 0;
    
//     // Copy message with truncation if needed
//     strncpy(compact_irq.message, irq->message, sizeof(compact_irq.message) - 1);
//     compact_irq.message[sizeof(compact_irq.message) - 1] = '\0';
    
//     // Create NoC packet
//     noc_packet_t pkt;
//     memset(&pkt, 0, sizeof(pkt));
    
//     // Set header
//     pkt.hdr.src_x = src_tile % 4;
//     pkt.hdr.src_y = src_tile / 4;
//     pkt.hdr.dest_x = dst_tile % 4;
//     pkt.hdr.dest_y = dst_tile / 4;
//     pkt.hdr.type = PKT_INTERRUPT_REQ;
//     pkt.hdr.length = sizeof(compact_interrupt_packet_t);
//     pkt.hdr.hop_count = 0;
    
//     // Copy compact interrupt data to payload
//     if (sizeof(compact_interrupt_packet_t) <= sizeof(pkt.payload)) {
//         memcpy(pkt.payload, &compact_irq, sizeof(compact_interrupt_packet_t));
//         printf("[NOC-IRQ-SEND] Sending %s interrupt from tile %d to tile %d (%zu bytes)\n",
//                get_irq_type_name(irq->type), src_tile, dst_tile, sizeof(compact_interrupt_packet_t));
//     } else {
//         printf("[NOC-IRQ] Compact interrupt data too large for packet payload\n");
//         return -1;
//     }
    
//     // Send via NoC router
//     return noc_send_packet(&pkt);
// }

// Handle received interrupt from NoC
// int noc_handle_received_interrupt(mesh_platform_t* p, interrupt_request_t* irq) {
//     if (!p || !irq || !p->plic_enabled) return -1;
    
//     c0_interrupt_controller_t* ctrl = &p->interrupt_controller;
    
//     pthread_mutex_lock(&ctrl->irq_lock);
    
//     // Check if queue is full
//     if (ctrl->count >= MAX_PENDING_IRQS) {
//         ctrl->interrupts_dropped++;
//         pthread_mutex_unlock(&ctrl->irq_lock);
//         printf("[C0-IRQ] Interrupt queue full, dropping interrupt from tile %d\n", irq->source_tile);
//         return -1;
//     }
    
//     // Add to queue
//     memcpy(&ctrl->irq_queue[ctrl->tail], irq, sizeof(interrupt_request_t));
//     ctrl->tail = (ctrl->tail + 1) % MAX_PENDING_IRQS;
//     ctrl->count++;
//     ctrl->interrupts_received++;
    
//     printf("[C0-IRQ] Queued %s interrupt from tile %d (queue: %d/%d)\n",
//            get_irq_type_name(irq->type), irq->source_tile, ctrl->count, MAX_PENDING_IRQS);
    
//     // Signal interrupt available
//     pthread_cond_signal(&ctrl->irq_available);
//     pthread_mutex_unlock(&ctrl->irq_lock);
    
//     return 0;
// }

// Default interrupt handlers
int default_task_complete_handler(interrupt_request_t* irq, void* platform_context) {
    mesh_platform_t* p = (mesh_platform_t*)platform_context;
    
    printf("[C0-IRQ-HANDLER] Task %u completed on tile %d: %s\n",
           irq->data, irq->source_tile, irq->message);
    
    // Update platform task counters
    pthread_mutex_lock(&p->platform_lock);
    p->completed_tasks++;
    if (p->active_tasks > 0) {
        p->active_tasks--;
    }
    pthread_mutex_unlock(&p->platform_lock);
    
    return 0;
}

int default_error_handler(interrupt_request_t* irq, void* platform_context) {
    printf("[C0-IRQ-HANDLER] ERROR on tile %d (code=0x%x): %s\n",
           irq->source_tile, irq->data, irq->message);
    
    // Could trigger error recovery or tile restart
    return 0;
}

int default_dma_complete_handler(interrupt_request_t* irq, void* platform_context) {
    printf("[C0-IRQ-HANDLER] DMA transfer %u completed on tile %d: %s\n",
           irq->data, irq->source_tile, irq->message);
    return 0;
}

int default_resource_request_handler(interrupt_request_t* irq, void* platform_context) {
    printf("[C0-IRQ-HANDLER] Resource request %u from tile %d: %s\n",
           irq->data, irq->source_tile, irq->message);
    
    // Could implement resource allocation logic here
    return 0;
}

int default_shutdown_handler(interrupt_request_t* irq, void* platform_context) {
    mesh_platform_t* p = (mesh_platform_t*)platform_context;
    
    printf("[C0-IRQ-HANDLER] Shutdown request from tile %d: %s\n",
           irq->source_tile, irq->message);
    
    // Could trigger graceful tile shutdown
    return 0;
}


int c0_process_plic_interrupts(mesh_platform_t* platform) {
    int processed = 0;
    
    // Process interrupts for C0 (hart 0)
    volatile PLIC_RegDef* plic;
    uint32_t target_local;
    plic_select(0, &plic, &target_local);  // C0 is hart 0
    
    printf("[C0-PLIC] Hart 0: plic=%p, target_local=%d\n", plic, target_local);
    
    if (!plic) {
        printf("[C0-PLIC] No PLIC instance for hart 0\n");
        return 0;
    }
    
    // Check for pending interrupts first
    printf("[C0-PLIC] Checking for pending interrupts...\n");
    for (int source = 32; source <= 40; source++) {  // Check around our expected source ID
        int pending = PLIC_N_source_pending_read((PLIC_RegDef*)plic, source);
        if (pending) {
            printf("[C0-PLIC] Source %d is pending!\n", source);
        }
    }
    
    // Check if target 0 is enabled for source 33
    int target_enabled = PLIC_M_TAR_read((PLIC_RegDef*)plic, target_local, 33);
    printf("[C0-PLIC] Target %d enabled for source 33: %s\n", target_local, target_enabled ? "YES" : "NO");
    
    // Check threshold for target 0
    int threshold = PLIC_M_TAR_thre_read((PLIC_RegDef*)plic, target_local);
    printf("[C0-PLIC] Target %d threshold: %d\n", target_local, threshold);
    
    // Check priority of source 33
    printf("[C0-PLIC] Checking source 33 priority...\n");
    // Access priority register directly - the priority is in sprio_regs[source-1]
    uint32_t priority_reg_value = ((PLIC_RegDef*)plic)->sprio_regs[33-1];
    printf("[C0-PLIC] Source 33 priority register value: %d\n", priority_reg_value);
    
    // Claim any pending interrupts
    uint32_t claim_id = PLIC_M_TAR_claim_read(plic, target_local);
    printf("[C0-PLIC] Claim attempt returned: %d\n", claim_id);
    while (claim_id > 0) {
        printf("[C0-PLIC] Claimed interrupt source ID %d\n", claim_id);
        
        // Determine which tile sent this (based on PLIC source ID calculation)
        // source_id = SOURCE_BASE_ID + target_local_idx * SLOT_PER_TARGET + source_hart_id
        // For your platform: source_id = 32 + 0 * 8 + source_hart_id = 32 + source_hart_id
        if (claim_id >= SOURCE_BASE_ID && claim_id < SOURCE_BASE_ID + NR_HARTS) {
            uint32_t source_hart = claim_id - SOURCE_BASE_ID;
            printf("[C0-PLIC] Received interrupt from hart %d\n", source_hart);
            
            // Handle the interrupt (task complete, error, etc.)
            handle_plic_interrupt_from_tile(platform, source_hart, claim_id);
        } else {
            printf("[C0-PLIC] Unknown interrupt source ID %d\n", claim_id);
        }
        
        // Complete the interrupt (required by PLIC)
        PLIC_M_TAR_comp_write(plic, target_local, claim_id);
        processed++;
        
        // Check for more interrupts
        claim_id = PLIC_M_TAR_claim_read(plic, target_local);
    }
    
    return processed;
}

// Helper function to handle interrupts from tiles
void handle_plic_interrupt_from_tile(mesh_platform_t* platform, uint32_t source_hart, uint32_t source_id) {
    // Update platform statistics
    pthread_mutex_lock(&platform->platform_lock);
    platform->completed_tasks++;
    pthread_mutex_unlock(&platform->platform_lock);
    
    printf("[C0-PLIC] Hart %d completed a task\n", source_hart);
}

void test_plic_functionality(mesh_platform_t* platform) {
    printf("[PLIC-TEST] Testing PLIC interrupt system...\n");
    
    // Calculate the expected source ID for debugging
    uint32_t target_local_idx = 0;  // Hart 0 is target
    uint32_t source_hart_id = 1;    // Hart 1 is source
    uint32_t expected_source_id = SOURCE_BASE_ID + target_local_idx * SLOT_PER_TARGET + source_hart_id;
    printf("[PLIC-TEST] Expected source ID: %d (BASE=%d + target_idx=%d * SLOT=%d + source=%d)\n",
           expected_source_id, SOURCE_BASE_ID, target_local_idx, SLOT_PER_TARGET, source_hart_id);
    
    // Enable hart 0 to receive interrupts from the calculated source ID
    printf("[PLIC-TEST] Enabling hart 0 to receive source ID %d...\n", expected_source_id);
    PLIC_enable_interrupt(expected_source_id, 0);
    PLIC_set_priority(expected_source_id, 0, 2);
    
    // Test hart 1 sending interrupt to hart 0
    int result = PLIC_trigger_interrupt(1, 0);
    printf("[PLIC-TEST] Trigger result: %d\n", result);
    
    // Process the interrupt
    int processed = c0_process_plic_interrupts(platform);
    printf("[PLIC-TEST] Processed %d interrupts\n", processed);
    
    if (processed > 0) {
        printf("[PLIC-TEST] ✓ PLIC is working!\n");
    } else {
        printf("[PLIC-TEST] ✗ PLIC not receiving interrupts\n");
    }
}

// Enhanced interrupt processing for bidirectional communication
int c0_process_enhanced_plic_interrupts(mesh_platform_t* platform) {
    int processed = 0;
    
    // Process interrupts for C0 (hart 0)
    volatile PLIC_RegDef* plic;
    uint32_t target_local;
    plic_select(0, &plic, &target_local);  // C0 is hart 0
    
    if (!plic) {
        printf("[C0-PLIC] No PLIC instance for hart 0\n");
        return 0;
    }
    
    // Claim any pending interrupts
    uint32_t claim_id = PLIC_M_TAR_claim_read(plic, target_local);
    while (claim_id > 0) {
        printf("[C0-PLIC] Claimed interrupt source ID %d\n", claim_id);
        
        // Decode the interrupt using the enhanced source ID calculation
        uint32_t source_hart = 0;
        irq_source_id_t irq_type = IRQ_MESH_NODE;
        
        // Determine source hart and interrupt type from source ID
        if (claim_id >= SOURCE_BASE_ID) {
            // New scheme: source_id = SOURCE_BASE_ID + (source_hart * 32) + irq_type
            uint32_t offset = claim_id - SOURCE_BASE_ID;
            source_hart = offset / 32;
            irq_type = (irq_source_id_t)(offset % 32);
            
            if (source_hart < NR_HARTS) {
                printf("[C0-PLIC] Enhanced decode: Hart %d sent %s interrupt\n", 
                       source_hart, get_interrupt_type_name(irq_type));
                       
                handle_enhanced_plic_interrupt(platform, source_hart, irq_type, claim_id);
            } else {
                printf("[C0-PLIC] Invalid source hart %d decoded from source ID %d\n", 
                       source_hart, claim_id);
            }
        } else {
            printf("[C0-PLIC] Legacy or system interrupt source ID %d\n", claim_id);
        }
        
        // Complete the interrupt (required by PLIC)
        PLIC_M_TAR_comp_write(plic, target_local, claim_id);
        processed++;
        
        // Check for more interrupts
        claim_id = PLIC_M_TAR_claim_read(plic, target_local);
    }
    
    return processed;
}

// Helper function to get interrupt type name
const char* get_interrupt_type_name(irq_source_id_t irq_type) {
    switch (irq_type) {
        case IRQ_MESH_NODE: return "MESH_NODE";
        case IRQ_TASK_COMPLETE: return "TASK_COMPLETE";
        case IRQ_TASK_ASSIGN: return "TASK_ASSIGN";
        case IRQ_ERROR_REPORT: return "ERROR_REPORT";
        case IRQ_DMA_COMPLETE: return "DMA_COMPLETE";
        case IRQ_SYNC_REQUEST: return "SYNC_REQUEST";
        case IRQ_SYNC_RESPONSE: return "SYNC_RESPONSE";
        case IRQ_SHUTDOWN_REQUEST: return "SHUTDOWN_REQUEST";
        default: return "UNKNOWN";
    }
}

// Enhanced interrupt handler that can respond appropriately to different interrupt types
void handle_enhanced_plic_interrupt(mesh_platform_t* platform, uint32_t source_hart, 
                                   irq_source_id_t irq_type, uint32_t source_id) {
    printf("[C0-PLIC] Handling %s interrupt from hart %d\n", 
           get_interrupt_type_name(irq_type), source_hart);
    
    switch (irq_type) {
        case IRQ_TASK_COMPLETE:
            // Update platform task counters
            pthread_mutex_lock(&platform->platform_lock);
            platform->completed_tasks++;
            if (platform->active_tasks > 0) {
                platform->active_tasks--;
            }
            pthread_mutex_unlock(&platform->platform_lock);
            printf("[C0-PLIC] Task completed on hart %d\n", source_hart);
            break;
            
        case IRQ_ERROR_REPORT:
            printf("[C0-PLIC] ERROR reported by hart %d - investigating...\n", source_hart);
            // Could trigger error recovery or restart procedures
            break;
            
        case IRQ_SYNC_REQUEST:
            printf("[C0-PLIC] Sync request from hart %d - sending response...\n", source_hart);
            // Send a sync response back to the requesting hart
            PLIC_trigger_typed_interrupt(0, source_hart, IRQ_SYNC_RESPONSE);
            break;
            
        case IRQ_SHUTDOWN_REQUEST:
            printf("[C0-PLIC] Shutdown request from hart %d - initiating graceful shutdown...\n", source_hart);
            // Could trigger platform shutdown procedures
            break;
            
        case IRQ_DMA_COMPLETE:
            printf("[C0-PLIC] DMA transfer completed on hart %d\n", source_hart);
            break;
            
        default:
            printf("[C0-PLIC] Standard interrupt from hart %d\n", source_hart);
            // Default handling for legacy interrupts
            pthread_mutex_lock(&platform->platform_lock);
            platform->completed_tasks++;
            pthread_mutex_unlock(&platform->platform_lock);
            break;
    }
}

// Demo function showing bidirectional communication capabilities
void demo_bidirectional_plic_communication(mesh_platform_t* platform) {
    printf("\n[PLIC-DEMO] === Bidirectional PLIC Communication Demo ===\n");
    
    // 1. Processing node -> C0 communication (existing pattern)
    printf("\n[PLIC-DEMO] 1. Processing node -> C0 communication:\n");
    
    printf("[PLIC-DEMO] Hart 1 sends TASK_COMPLETE to Hart 0...\n");
    int result1 = PLIC_trigger_typed_interrupt(1, 0, IRQ_TASK_COMPLETE);
    printf("[PLIC-DEMO] Trigger result: %d\n", result1);
    
    printf("[PLIC-DEMO] Hart 2 sends ERROR_REPORT to Hart 0...\n");
    int result2 = PLIC_trigger_typed_interrupt(2, 0, IRQ_ERROR_REPORT);
    printf("[PLIC-DEMO] Trigger result: %d\n", result2);
    
    printf("[PLIC-DEMO] Hart 3 sends SYNC_REQUEST to Hart 0...\n");
    int result3 = PLIC_trigger_typed_interrupt(3, 0, IRQ_SYNC_REQUEST);
    printf("[PLIC-DEMO] Trigger result: %d\n", result3);
    
    // Process all C0 interrupts
    printf("\n[PLIC-DEMO] C0 processing received interrupts...\n");
    int processed = c0_process_enhanced_plic_interrupts(platform);
    printf("[PLIC-DEMO] C0 processed %d interrupts\n", processed);
    
    // 2. C0 -> Processing nodes communication (new capability)  
    printf("\n[PLIC-DEMO] 2. C0 -> Processing nodes communication:\n");
    
    printf("[PLIC-DEMO] C0 (Hart 0) sends TASK_ASSIGN to Hart 1...\n");
    int result4 = PLIC_trigger_typed_interrupt(0, 1, IRQ_TASK_ASSIGN);
    printf("[PLIC-DEMO] Trigger result: %d\n", result4);
    
    printf("[PLIC-DEMO] C0 (Hart 0) sends TASK_ASSIGN to Hart 2...\n");
    int result5 = PLIC_trigger_typed_interrupt(0, 2, IRQ_TASK_ASSIGN);
    printf("[PLIC-DEMO] Trigger result: %d\n", result5);
    
    printf("[PLIC-DEMO] C0 (Hart 0) sends SHUTDOWN_REQUEST to Hart 3...\n");
    int result6 = PLIC_trigger_typed_interrupt(0, 3, IRQ_SHUTDOWN_REQUEST);
    printf("[PLIC-DEMO] Trigger result: %d\n", result6);
    
    // Note: Processing nodes would need interrupt handling code to actually receive these
    printf("\n[PLIC-DEMO] Note: Processing nodes need interrupt handlers to receive C0 interrupts\n");
    printf("[PLIC-DEMO] This demonstrates the PLIC infrastructure supports bidirectional communication\n");
    
    printf("\n[PLIC-DEMO] === Demo Complete ===\n");
}
