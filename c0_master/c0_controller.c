#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "c0_controller.h"
#include "hal_tests/test_framework.h"

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
    
    // Basic processor loop - just idle for now
    while (tile->running) {
        // STEP 1: Simple idle loop
        // In Phase 2, this will handle tasks from C0 master
        usleep(1000); // 1ms idle sleep
        
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
    printf("\n== Mesh‑NoC HAL Validation (Step 2: Task Infrastructure) ==\n");
    
    // STEP 1: Start tile processor threads (main thread becomes C0 master)
    if (platform_start_tile_threads(platform) == 0) {
        printf("[C0 Master] Platform running with tile processors and task system!\n");
        
        // C0 master supervises the platform
        c0_master_supervise_tiles(platform);
        
        // Run the existing test suite (C0 master coordinates)
        printf("[C0 Master] Executing test suite...\n");
        run_all_tests(platform);
        
        // STEP 1: Stop tile processor threads
        platform_stop_tile_threads(platform);
    } else {
        printf("[C0 Master] ERROR: Failed to start tile threads, running in single-threaded mode\n");
        run_all_tests(platform);
    }
}
