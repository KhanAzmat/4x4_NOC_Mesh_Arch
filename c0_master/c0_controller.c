#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "c0_controller.h"
#include "hal_tests/test_framework.h"

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

// STEP 1: Basic tile processor main loop
void* tile_processor_main(void* arg)
{
    tile_core_t* tile = (tile_core_t*)arg;
    
    printf("[Tile %d] Starting processor thread...\n", tile->id);
    
    // Initialize tile state
    pthread_mutex_lock(&tile->state_lock);
    tile->running = true;
    tile->initialized = true;
    
    // STEP 2: Initialize task execution state
    tile->current_task = NULL;
    tile->task_pending = false;
    tile->idle = true;
    tile->tasks_completed = 0;
    tile->total_execution_time = 0;
    
    pthread_mutex_unlock(&tile->state_lock);
    
    // STEP 2: Enhanced processor loop with task execution
    while (tile->running) {
        // Try to get next task from C0 master
        // We need platform context - use extern declaration for now
        extern mesh_platform_t* g_platform_context;
        mesh_platform_t* platform = g_platform_context;
        
        if (platform) {
            task_t* task = tile_get_next_task(platform, tile);
            
            if (task) {
                // Execute the task
                tile_execute_task(tile, task);
                
                // Report completion to C0 master
                tile_complete_task(platform, tile, task);
            } else {
                // No tasks available, idle
                pthread_mutex_lock(&tile->state_lock);
                tile->idle = true;
                pthread_mutex_unlock(&tile->state_lock);
                
                usleep(1000); // 1ms idle sleep
            }
        } else {
            usleep(1000); // 1ms idle sleep if no platform context
        }
        
        // Check if we should stop
        pthread_mutex_lock(&tile->state_lock);
        bool should_run = tile->running;
        pthread_mutex_unlock(&tile->state_lock);
        
        if (!should_run) break;
    }
    
    printf("[Tile %d] Processor thread stopping...\n", tile->id);
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
    
    // Initialize all tile threads
    for (int i = 0; i < p->node_count; i++) {
        tile_core_t* tile = &p->nodes[i];
        
        // Initialize thread state
        pthread_mutex_init(&tile->state_lock, NULL);
        tile->running = false;
        tile->initialized = false;
        
        // Create tile thread
        if (pthread_create(&tile->thread_id, NULL, tile_processor_main, tile) != 0) {
            printf("[C0 Master] ERROR: Failed to create thread for tile %d\n", i);
            return -1;
        }
    }
    
    // Wait for all tiles to initialize
    printf("[C0 Master] Waiting for tile threads to initialize...\n");
    
    // Wait for all tiles to initialize
    for (int i = 0; i < p->node_count; i++) {
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
    
    // Signal all tile threads to stop
    for (int i = 0; i < p->node_count; i++) {
        pthread_mutex_lock(&p->nodes[i].state_lock);
        p->nodes[i].running = false;
        pthread_mutex_unlock(&p->nodes[i].state_lock);
    }
    
    // Wait for all tile threads to finish
    for (int i = 0; i < p->node_count; i++) {
        pthread_join(p->nodes[i].thread_id, NULL);
    }
    
    // STEP 2: Clean up task system
    task_queue_destroy(&p->task_queue);
    pthread_mutex_destroy(&p->task_id_lock);
    
    // Clean up mutexes
    for (int i = 0; i < p->node_count; i++) {
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
    printf("[C0 Master] Supervising tile processors...\n");
    
    // Simple supervision - in later steps this will coordinate tasks
    int supervision_cycles = 0;
    
    while (p->platform_running && supervision_cycles < 5) {
        // Check tile status
        int active_tiles = 0;
        int idle_tiles = 0;
        int total_completed_tasks = 0;
        
        for (int i = 0; i < p->node_count; i++) {
            pthread_mutex_lock(&p->nodes[i].state_lock);
            if (p->nodes[i].running) {
                active_tiles++;
                if (p->nodes[i].idle) {
                    idle_tiles++;
                }
                total_completed_tasks += p->nodes[i].tasks_completed;
            }
            pthread_mutex_unlock(&p->nodes[i].state_lock);
        }
        
        printf("[C0 Master] Supervision cycle %d: %d tiles active, %d idle, %d total tasks completed\n", 
               supervision_cycles + 1, active_tiles, idle_tiles, total_completed_tasks);
        
        usleep(200000); // 200ms supervision interval
        supervision_cycles++;
    }
    
    printf("[C0 Master] Supervision complete\n");
}

// Enhanced test runner with main thread as C0 master
void c0_run_test_suite(mesh_platform_t* platform)
{
    printf("\n== Mesh‑NoC HAL Validation (Step 2: Distributed HAL Tests) ==\n");
    
    // STEP 1: Start tile processor threads (main thread becomes C0 master)
    if (platform_start_tile_threads(platform) == 0) {
        printf("[C0 Master] Platform running with tile processors and task system!\n");
        
        // C0 master supervises the platform
        c0_master_supervise_tiles(platform);
        
        // STEP 2: Run HAL tests distributed across tiles
        printf("[C0 Master] Executing HAL tests distributed across tiles...\n");
        c0_run_hal_tests_distributed(platform);
        
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

// Function to begin atomic print session (blocks other threads from printing)
void begin_print_session(int tile_id, const char* task_name) {
    pthread_mutex_lock(&print_session_lock);
    printf("=== [Tile %d] Starting %s - Print Session BEGIN ===\n", tile_id, task_name);
    fflush(stdout);
}

// Function to end atomic print session (allows other threads to print)
void end_print_session(int tile_id, const char* task_name, int result) {
    printf("=== [Tile %d] %s Completed: %s - Print Session END ===\n\n", 
           tile_id, task_name, result ? "PASS" : "FAIL");
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
    
    return 0;
}

int wait_for_all_tasks_completion(mesh_platform_t* p, int expected_count)
{
    if (!p || expected_count <= 0) {
        return -1;
    }
    
    printf("[C0 Master] Waiting for %d HAL test tasks to complete...\n", expected_count);
    
    int completed = 0;
    while (completed < expected_count) {
        // Check completion status
        pthread_mutex_lock(&p->platform_lock);
        completed = p->completed_tasks;
        pthread_mutex_unlock(&p->platform_lock);
        
        if (completed < expected_count) {
            usleep(10000); // 10ms check interval
        }
    }
    
    printf("[C0 Master] All %d HAL test tasks completed!\n", expected_count);
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

void c0_run_hal_tests_distributed(mesh_platform_t* platform)
{
    printf("[C0 Master] === Running Tests: C0 Master + Distributed HAL ===\n");
    
    // STEP 1: Run C0 Master tests on main thread (these are C0 coordination tasks)
    printf("[C0 Master] Executing C0 Master coordination tests...\n");
    extern int test_c0_gather(mesh_platform_t* p);
    extern int test_c0_distribute(mesh_platform_t* p);
    
    int c0_gather_result = test_c0_gather(platform);
    int c0_distribute_result = test_c0_distribute(platform);
    
    printf("[C0 Master] C0 Master tests completed:\n");
    printf("[C0 Master] - C0 Gather: %s\n", c0_gather_result ? "PASS" : "FAIL");
    printf("[C0 Master] - C0 Distribute: %s\n", c0_distribute_result ? "PASS" : "FAIL");
    printf("\n");
    
    // STEP 2: Run HAL tests distributed across tile processors IN PARALLEL
    printf("[C0 Master] Executing HAL tests in parallel across tile processors...\n");
    
    // Test function table using wrapper functions (excluding C0 tests)
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
        {hal_test_random_dma_remote_wrapper, "Random DMA Remote", 0}
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
    main_thread_print("[C0 Master] === Test Results Summary ===\n");
    main_thread_print("[C0 Master] C0 Master Tests (Main Thread):\n");
    main_thread_print("[C0 Master] - C0 Gather: %s\n", c0_gather_result ? "PASS" : "FAIL");
    main_thread_print("[C0 Master] - C0 Distribute: %s\n", c0_distribute_result ? "PASS" : "FAIL");
    
    int hal_passed = 0;
    main_thread_print("[C0 Master] HAL Tests (Parallel Distribution to Tile Processors):\n");
    for (int i = 0; i < num_hal_tests; i++) {
        hal_passed += hal_tests[i].result;
        main_thread_print("[C0 Master] - %s: %s\n", hal_tests[i].name, hal_tests[i].result ? "PASS" : "FAIL");
    }
    
    int total_passed = c0_gather_result + c0_distribute_result + hal_passed;
    int total_tests = 2 + num_hal_tests;
    main_thread_print("[C0 Master] Overall Summary: %d/%d tests passed\n", total_passed, total_tests);
    main_thread_print("[C0 Master] === Test Execution Complete ===\n\n");
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
        
        if (task->assigned_tile == tile->id && !task->completed && task->result == 0) {
            // Found a task assigned to this tile that hasn't been started
            // Mark it as started by setting result to -1 temporarily
            if (task->result == 0) {
                task->result = -1; // Mark as in-progress
                assigned_task = task;
                break;
            }
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
            // BEGIN ATOMIC PRINT SESSION - blocks other threads from printing
            begin_print_session(tile->id, task->params.hal_test.test_name);
            
            // Execute real HAL test function within atomic print session
            printf("[Tile %d] Retrieved assigned task %d (%s)\n", 
                   tile->id, task->task_id, task->params.hal_test.test_name);
            printf("[Tile %d] Executing task %d (type %d)...\n", tile->id, task->task_id, task->type);
            printf("[Tile %d] Executing HAL test: %s\n", tile->id, task->params.hal_test.test_name);
            
            if (task->params.hal_test.test_func && task->params.hal_test.platform) {
                // Call the HAL test function with platform parameter
                result = task->params.hal_test.test_func(task->params.hal_test.platform);
                
                // Store result in the pointer location for main thread to read
                if (task->params.hal_test.result_ptr) {
                    *(task->params.hal_test.result_ptr) = result;
                }
                
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
            printf("[Tile %d] Executing memory copy task...\n", tile->id);
            usleep(5000); // 5ms simulated work
            result = 256; // Simulate 256 bytes copied
            break;
            
        case TASK_TYPE_DMA_TRANSFER:
            // DMA transfer simulation
            printf("[Tile %d] Executing DMA transfer task...\n", tile->id);
            usleep(8000); // 8ms simulated work
            result = (int)task->params.memory_op.size;
            break;
            
        case TASK_TYPE_COMPUTATION:
            // Computation simulation
            printf("[Tile %d] Executing computation task...\n", tile->id);
            usleep(15000); // 15ms simulated work
            result = 1; // Success
            break;
            
        case TASK_TYPE_NOC_TRANSFER:
            // NoC transfer simulation
            printf("[Tile %d] Executing NoC transfer task...\n", tile->id);
            usleep(10000); // 10ms simulated work
            result = (int)task->params.memory_op.size;
            break;
            
        case TASK_TYPE_TEST_EXECUTION:
            // Test execution simulation
            printf("[Tile %d] Executing test task %d...\n", tile->id, task->params.test_exec.test_id);
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
